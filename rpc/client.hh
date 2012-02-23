#include "rpc/protocol.hh"

namespace ten {

class rpc_client : public boost::noncopyable {
public:
    rpc_client(const std::string &hostname_, uint16_t port_)
        : hostname(hostname_), port(port_), msgid(0)
    {
    }

    template <typename Result, typename ...Args>
        Result call(const std::string &method, Args ...args) {
            uint32_t mid = ++msgid;
            return rpcall<Result>(mid, method, args...);
        }

protected:
    netsock s;
    std::string hostname;
    uint64_t port;
    uint32_t msgid;
    msgpack::unpacker pac;

    void ensure_connection() {
        if (!s.valid()) {
            netsock tmp(AF_INET, SOCK_STREAM);
            std::swap(s, tmp);
            if (s.dial(hostname.c_str(), port) != 0) {
                throw errorx("rpc client connection failed");
            }
        }
    }

    template <typename Result>
        Result rpcall(msgpack::packer<msgpack::sbuffer> &pk, msgpack::sbuffer &sbuf) {
            ensure_connection();
            ssize_t nw = s.send(sbuf.data(), sbuf.size());
            if (nw != (ssize_t)sbuf.size()) {
                s.close();
                throw errorx("rpc call failed to send");
            }

            size_t bsize = 4096;

            for (;;) {
                pac.reserve_buffer(bsize);
                ssize_t nr = s.recv(pac.buffer(), bsize);
                if (nr <= 0) {
                    s.close();
                    throw errorx("rpc client lost connection");
                }
                DVLOG(3) << "client recv: " << nr;
                pac.buffer_consumed(nr);

                msgpack::unpacked result;
                if (pac.next(&result)) {
                    msgpack::object o = result.get();
                    DVLOG(3) << "client got: " << o;
                    msg_response<msgpack::object, msgpack::object> resp;
                    o.convert(&resp);
                    if (resp.error.is_nil()) {
                        return resp.result.as<Result>();
                    } else {
                        LOG(ERROR) << "rpc error returned: " << resp.error;
                        throw errorx(resp.error.as<std::string>());
                    }
                }
            }
            // shouldn't get here.
            throw errorx("rpc client unknown error");
        }

    template <typename Result, typename Arg, typename ...Args>
        Result rpcall(msgpack::packer<msgpack::sbuffer> &pk, msgpack::sbuffer &sbuf, Arg arg, Args ...args) {
            pk.pack(arg);
            return rpcall<Result>(pk, sbuf, args...);
        }

    template <typename Result, typename ...Args>
        Result rpcall(uint32_t msgid, const std::string &method, Args ...args) {
            msgpack::sbuffer sbuf;
            msgpack::packer<msgpack::sbuffer> pk(&sbuf);
            pk.pack_array(4);
            pk.pack_uint8(0); // request message type
            pk.pack_uint32(msgid);
            pk.pack(method);
            pk.pack_array(sizeof...(args));
            return rpcall<Result>(pk, sbuf, args...);
        }
};

} // end namespace ten
