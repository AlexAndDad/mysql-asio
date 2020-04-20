//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_GENERIC_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_GENERIC_HPP

#include <boost/asio/yield.hpp>
#include <limits>

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType>
class execute_processor
{
	deserialize_row_fn deserializer_;
	channel<StreamType>& channel_;
	bytestring buffer_;
	std::size_t field_count_ {};
	ok_packet ok_packet_;
	std::vector<field_metadata> fields_;
	std::vector<bytestring> field_buffers_;
public:
	execute_processor(deserialize_row_fn deserializer, channel<StreamType>& chan):
		deserializer_(deserializer), channel_(chan) {};

	template <typename Serializable>
	void process_request(
		const Serializable& request
	)
	{
		// Serialize the request
		capabilities caps = channel_.current_capabilities();
		serialize_message(request, caps, buffer_);

		// Prepare the channel
		channel_.reset_sequence_number();
	}

	void process_response(
		error_code& err,
		error_info& info
	)
	{
		// Response may be: ok_packet, err_packet, local infile request (not implemented)
		// If it is none of this, then the message type itself is the beginning of
		// a length-encoded int containing the field count
		deserialization_context ctx (boost::asio::buffer(buffer_), channel_.current_capabilities());
		std::uint8_t msg_type;
		std::tie(err, msg_type) = deserialize_message_type(ctx);
		if (err) return;
		if (msg_type == ok_packet_header)
		{
			err = deserialize_message(ok_packet_, ctx);
			if (err) return;
			field_count_ = 0;
		}
		else if (msg_type == error_packet_header)
		{
			err = process_error_packet(ctx, info);
		}
		else
		{
			// Resultset with metadata. First packet is an int_lenenc with
			// the number of field definitions to expect. Message type is part
			// of this packet, so we must rewind the context
			ctx.rewind(1);
			int_lenenc num_fields;
			err = deserialize_message(num_fields, ctx);
			if (err) return;

			// For platforms where size_t is shorter than uint64_t,
			// perform range check
			if (num_fields.value > std::numeric_limits<std::size_t>::max())
			{
				err = make_error_code(errc::protocol_value_error);
				return;
			}

			// Ensure we have fields, as field_count is indicative of
			// a resultset with fields
			field_count_ = static_cast<std::size_t>(num_fields.value);
			if (field_count_ == 0)
			{
				err = make_error_code(errc::protocol_value_error);
				return;
			}

			fields_.reserve(field_count_);
			field_buffers_.reserve(field_count_);
		}
	}

	error_code process_field_definition()
	{
		column_definition_packet field_definition;
		deserialization_context ctx (boost::asio::buffer(buffer_), channel_.current_capabilities());
		auto err = deserialize_message(field_definition, ctx);
		if (err) return err;

		// Add it to our array
		fields_.push_back(field_definition);
		field_buffers_.push_back(std::move(buffer_));
		buffer_ = bytestring();

		return error_code();
	}

	resultset<StreamType> create_resultset() &&
	{
		if (field_count_ == 0)
		{
			return resultset<StreamType>(
				channel_,
				std::move(buffer_),
				ok_packet_
			);
		}
		else
		{
			return resultset<StreamType>(
				channel_,
				resultset_metadata(std::move(field_buffers_), std::move(fields_)),
				deserializer_
			);
		}
	}

	auto& get_channel() { return channel_; }
	auto& get_buffer() { return buffer_; }

	std::size_t field_count() const noexcept { return field_count_; }
};

} // detail
} // mysql
} // boost

template <typename StreamType, typename Serializable>
void boost::mysql::detail::execute_generic(
	deserialize_row_fn deserializer,
	channel<StreamType>& channel,
	const Serializable& request,
	resultset<StreamType>& output,
	error_code& err,
	error_info& info
)
{
	// Compose a com_query message, reset seq num
	execute_processor<StreamType> processor (deserializer, channel);
	processor.process_request(request);

	// Send it
	channel.write(boost::asio::buffer(processor.get_buffer()), err);
	if (err) return;

	// Read the response
	channel.read(processor.get_buffer(), err);
	if (err) return;

	// Response may be: ok_packet, err_packet, local infile request (not implemented), or response with fields
	processor.process_response(err, info);
	if (err) return;

	// Read all of the field definitions (zero if empty resultset)
	for (std::uint64_t i = 0; i < processor.field_count(); ++i)
	{
		// Read the field definition packet
		channel.read(processor.get_buffer(), err);
		if (err) return;

		// Process the message
		err = processor.process_field_definition();
		if (err) return;
	}

	// No EOF packet is expected here, as we require deprecate EOF capabilities
	output = std::move(processor).create_resultset();
	err.clear();
}


template <typename StreamType, typename Serializable, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	boost::mysql::detail::execute_generic_signature<StreamType>
)
boost::mysql::detail::async_execute_generic(
	deserialize_row_fn deserializer,
	channel<StreamType>& chan,
	const Serializable& request,
	CompletionToken&& token,
	error_info* info
)
{
	using HandlerSignature = execute_generic_signature<StreamType>;
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;
	using ResultsetType = resultset<StreamType>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op: BaseType, boost::asio::coroutine
	{
		std::shared_ptr<execute_processor<StreamType>> processor_;
		std::uint64_t remaining_fields_ {0};
		error_info* output_info_;

		Op(
			HandlerType&& handler,
			deserialize_row_fn deserializer,
			channel<StreamType>& channel,
			const Serializable& request,
			error_info* output_info
		):
			BaseType(std::move(handler), channel.next_layer().get_executor()),
			processor_(std::make_shared<execute_processor<StreamType>>(deserializer, channel)),
			output_info_(output_info)
		{
			processor_->process_request(request);
		}

		void operator()(
			error_code err,
			bool cont=true
		)
		{
			// Error checking
			if (err)
			{
				this->complete(cont, err, ResultsetType());
				return;
			}

			// Non-error path
			error_info info;
			reenter(*this)
			{
				// The request message has already been composed in the ctor. Send it
				yield processor_->get_channel().async_write(
					boost::asio::buffer(processor_->get_buffer()),
					std::move(*this)
				);

				// Read the response
				yield processor_->get_channel().async_read(
					processor_->get_buffer(),
					std::move(*this)
				);

				// Response may be: ok_packet, err_packet, local infile request (not implemented), or response with fields
				processor_->process_response(err, info);
				if (err)
				{
					conditional_assign(output_info_, std::move(info));
					this->complete(cont, err, ResultsetType());
					yield break;
				}
				remaining_fields_ = processor_->field_count();

				// Read all of the field definitions
				while (remaining_fields_ > 0)
				{
					// Read the field definition packet
					yield processor_->get_channel().async_read(
						processor_->get_buffer(),
						std::move(*this)
					);

					// Process the message
					err = processor_->process_field_definition();
					if (err)
					{
						this->complete(cont, err, ResultsetType());
						yield break;
					}

					remaining_fields_--;
				}

				// No EOF packet is expected here, as we require deprecate EOF capabilities
				this->complete(
					cont,
					error_code(),
					ResultsetType(std::move(*processor_).create_resultset())
				);
			}
		}
	};

	Op(
		std::move(initiator.completion_handler),
		deserializer,
		chan,
		request,
		info
	)(error_code(), false);
	return initiator.result.get();
}

#include <boost/asio/unyield.hpp>

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_IPP_ */
