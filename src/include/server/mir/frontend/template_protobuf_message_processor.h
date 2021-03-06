/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */


#ifndef MIR_FRONTEND_TEMPLATE_PROTOBUF_MESSAGE_PROCESSOR_H_
#define MIR_FRONTEND_TEMPLATE_PROTOBUF_MESSAGE_PROCESSOR_H_

#include "mir/frontend/message_processor.h"
#include "mir/cookie/authority.h"
#include "mir/client_visible_error.h"

#include "mir_protobuf.pb.h"

#include <google/protobuf/stubs/common.h>
#include <boost/exception/diagnostic_information.hpp>

#include <memory>
#include <string>

namespace mir
{
namespace frontend
{
namespace detail
{
// Utility metafunction result_ptr_t<> allows invoke() to pick the right
// send_response() overload. The base template resolves to the prototype
// "send_response(::google::protobuf::uint32 id, ::google::protobuf::Message* response)"
// Client code may specialize result_ptr_t to resolve to another overload.
template<typename ResultType> struct result_ptr_t
{ typedef ::google::protobuf::MessageLite* type; };

// Boiler plate for unpacking a parameter message, invoking a server function, and
// sending the result message. Assumes the existence of Self::send_response().
template<class Self, class Server, class ServerX, class ParameterMessage, class ResultMessage>
void invoke(
    Self* self,
    Server* server,
    void (ServerX::*function)(
        ParameterMessage const* request,
        ResultMessage* response,
        ::google::protobuf::Closure* done),
        Invocation const& invocation)
{
    ParameterMessage parameter_message;
    if (!parameter_message.ParseFromString(invocation.parameters()))
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to parse message parameters!"));
    ResultMessage result_message;

    try
    {
        std::unique_ptr<google::protobuf::Closure> callback(
            google::protobuf::NewPermanentCallback<
                Self,
                ::google::protobuf::uint32,
                typename result_ptr_t<ResultMessage>::type>(
                    self,
                    &Self::send_response,
                    invocation.id(),
                    &result_message));

        (server->*function)(
            &parameter_message,
            &result_message,
            callback.get());
    }
    catch (mir::cookie::SecurityCheckError const& /*err*/)
    {
        throw;
    }
    catch (mir::ClientVisibleError const& error)
    {
        auto client_error = result_message.mutable_structured_error();
        client_error->set_code(error.code());
        client_error->set_domain(error.domain());
        self->send_response(invocation.id(), &result_message);
    }
    catch (std::exception const& x)
    {
        using namespace std::literals::string_literals;
        result_message.set_error("Error processing request: "s +
            x.what() + "\nInternal error details: " + boost::diagnostic_information(x));
        self->send_response(invocation.id(), &result_message);
    }
}

}
}
}

#endif /* MIR_FRONTEND_TEMPLATE_PROTOBUF_MESSAGE_PROCESSOR_H_ */
