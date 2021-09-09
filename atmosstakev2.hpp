#include "novusphere.hpp"

using namespace eosio;
using namespace std;

CONTRACT atmosstakev2 : public eosio::contract
{
private:
    // ...
public:
    using eosio::contract::contract;

    //
    // TABLES
    //

    TABLE stake
    {
        uint64_t key;
        int64_t weight;
        eosio::public_key public_key;
        eosio::asset initial_balance;
        eosio::asset balance;
        eosio::time_point_sec expires;

        TABLE_PRIMARY_KEY(key);
        TABLE_SECONDARY_PUBLIC_KEY(public_key);
    };

    TABLE stat
    {
        int64_t total_weight;
        eosio::asset total_supply; // user funds
        eosio::asset subsidy_supply;
        eosio::asset round_subsidy;
        eosio::name token_contract;
        eosio::symbol token_symbol;
        eosio::time_point_sec last_claim;
        int64_t min_claim_secs;
        int64_t min_stake_secs;
        int64_t max_stake_secs;
        eosio::asset min_stake;

        TABLE_PRIMARY_KEY(token_symbol.raw());
    };

    TABLE account
    {
        uint64_t key;
        eosio::public_key public_key;
        eosio::asset total_balance;
        uint64_t total_weight;

        TABLE_PRIMARY_KEY(key);
        TABLE_SECONDARY_PUBLIC_KEY(public_key);
    };

    typedef eosio::multi_index<"stakes"_n, stake, eosio::index_by_public_key<stake>> stakes;
    typedef eosio::multi_index<"stats"_n, stat> stats;
    typedef eosio::multi_index<"accounts"_n, account, eosio::index_by_public_key<account>> accounts;

    //
    // ACTIONS
    //

    ACTION sanity();
    ACTION destroy();
    ACTION create(
        eosio::name token_contract,
        eosio::symbol token_symbol,
        eosio::asset round_subsidy,
        int64_t min_claim_secs,
        int64_t min_stake_secs,
        int64_t max_stake_secs,
        eosio::asset min_stake);
    ACTION exitstake(uint64_t key, eosio::symbol token_symbol, eosio::name to, string memo, eosio::signature sig);
    ACTION fexitstakes(eosio::symbol token_symbol, eosio::name stakes_to, eosio::name supply_to);
    ACTION claim(eosio::symbol token_symbol, eosio::name relay, string memo);
    ACTION resetclaim(eosio::symbol token_symbol);

    //
    // INTERNAL CALLED ACTIONS
    //

    void transfer(eosio::name from, eosio::name to, eosio::asset quantity, string memo);
    void stake(eosio::public_key public_key, eosio::asset balance, eosio::time_point_sec expires);
    void addsubsidy(eosio::asset balance);
};