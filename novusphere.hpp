#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/crypto.hpp>
#include <eosio/system.hpp>

#include <cstring>
#include <vector>

#define TABLE_PRIMARY_KEY(value) \
    uint64_t primary_key() const { return value; }

#define TABLE_SECONDARY_PUBLIC_KEY(pk) \
    eosio::fixed_bytes<32> bypk() const { return eosio::public_key_to_fixed_bytes(pk); }

#define ATMOS_SYMBOL (eosio::symbol("ATMOS", 3))
#define ATMOS_CONTRACT (eosio::name("novusphereio"))
#define MINUTE_IN_SECS (60)
#define DAY_IN_SECS (86400)
#define YEAR_IN_SECS (31536000)

#define by_public_key (eosio::name("bypk"))

extern bool b58tobin(void *bin, size_t *binszp, const char *b58);

namespace eosio
{
    using namespace std;

    template <typename T>
    struct index_by_public_key : public eosio::indexed_by<by_public_key, eosio::const_mem_fun<T, eosio::fixed_bytes<32>, &T::bypk>>
    {
    };

    inline vector<string> split_string(string s, string delimiter)
    {
        size_t pos_start = 0, pos_end, delim_len = delimiter.length();
        string token;
        vector<string> res;

        while ((pos_end = s.find(delimiter, pos_start)) != string::npos)
        {
            token = s.substr(pos_start, pos_end - pos_start);
            pos_start = pos_end + delim_len;
            res.push_back(token);
        }

        res.push_back(s.substr(pos_start));
        return res;
    }

    inline eosio::uint32_t time_diff_secs(eosio::time_point_sec tp1, eosio::time_point_sec tp2)
    {
        return tp1.sec_since_epoch() - tp2.sec_since_epoch();
    }

    inline eosio::time_point_sec current_time_point_sec()
    {
        return eosio::time_point_sec(eosio::current_time_point());
    }

    inline const eosio::fixed_bytes<32> public_key_to_fixed_bytes(const eosio::public_key publickey)
    {
        return eosio::sha256((const char *)publickey.data.begin(), 33);
    }

    inline const eosio::public_key public_key_from_string(const std::string str)
    {
        check(str.size() == 53, "str must be a 53-char EOS public key");
        check(str.substr(0, 3) == "EOS", "public key must start with EOS");

        eosio::public_key pk;
        size_t pk_len = 37;
        b58tobin((void *)pk.data.data(), &pk_len, str.substr(3).c_str());

        return pk;
    }

    template <name::raw A, typename B, typename... C>
    inline void clear_table(multi_index<A, B, C...> &table)
    {
        auto it = table.begin();
        while (it != table.end())
            it = table.erase(it);
    }

}; // namespace eosio