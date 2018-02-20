/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_CLIENT_ERROR_CHAIN_H
#define MIR_CLIENT_ERROR_CHAIN_H

#include "mir_presentation_chain.h"
#include <memory>

namespace mir
{
namespace client
{

class ErrorChain : public MirPresentationChain
{
public:
    ErrorChain(
        MirConnection* connection,
        int id,
        std::string const& error_msg);
    void submit_buffer(MirBuffer* buffer) override;
    MirConnection* connection() const override;
    int rpc_id() const override;
    char const* error_msg() const override;
    void set_dropping_mode() override;
    void set_queueing_mode() override;
private:
    MirConnection* const connection_;
    int const stream_id;
    std::string const error;
};

}
}
#endif /* MIR_CLIENT_ERROR_CHAIN_H */
