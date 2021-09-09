//
// Created by xia256 on behalf of the Novusphere Foundation
//

#include "atmosstakev2.hpp"

//
// Checks sanity of all data associated with the contract
//
ACTION atmosstakev2::sanity()
{
	// check public key sanity
	eosio::public_key pk = eosio::public_key_from_string("EOS82g6zVgPDNb1XDQBc6knEBusvPonq7KBhgCq3qkYWYt4kjm4JX");
	string pks = eosio::public_key_to_string(pk);
	eosio::checkf(pks == "EOS82g6zVgPDNb1XDQBc6knEBusvPonq7KBhgCq3qkYWYt4kjm4JX", "Unexpected key: %s != EOS82g6zVgPDNb1XDQBc6knEBusvPonq7KBhgCq3qkYWYt4kjm4JX", pks.c_str());

	// stats: total_weight, total_supply
	stats stats_table(_self, _self.value);

	for (auto it = stats_table.begin(); it != stats_table.end(); it++)
	{
		stakes stakes_table(_self, it->token_symbol.raw());
		accounts accounts_table(_self, it->token_symbol.raw());

		int64_t total_weight = 0;
		eosio::asset total_supply = eosio::asset(0, it->token_symbol);

		for (auto stake = stakes_table.begin(); stake != stakes_table.end(); stake++)
		{
			total_weight += stake->weight;
			total_supply += stake->balance;
		}

		eosio::checkf(total_weight == it->total_weight, "stat->total_weight=%s, [stakes_table]->total_weight=%s", to_string(it->total_weight).c_str(), to_string(total_weight).c_str());
		eosio::checkf(total_supply == it->total_supply, "stat->total_supply=%s, [stakes_table]->total_supply=%s", it->total_supply.to_string().c_str(), total_supply.to_string().c_str());

		total_weight = 0;
		total_supply = eosio::asset(0, it->token_symbol);

		for (auto acc = accounts_table.begin(); acc != accounts_table.end(); acc++)
		{
			total_weight += acc->total_weight;
			total_supply += acc->total_balance;
		}

		eosio::checkf(total_weight == it->total_weight, "stat->total_weight=%s, [stakes_table]->total_weight=%s", to_string(it->total_weight).c_str(), to_string(total_weight).c_str());
		eosio::checkf(total_supply == it->total_supply, "stat->total_supply=%s, [stakes_table]->total_supply=%s", it->total_supply.to_string().c_str(), total_supply.to_string().c_str());
	}

	eosio::check(false, "Sanity is OK");
}

//
// Destroys all data associated with the contract
// WARNING: should only be called upon termination or migration
//
ACTION atmosstakev2::destroy()
{
	eosio::require_auth(_self);

	stats stats_table(_self, _self.value);

	for (auto it = stats_table.begin(); it != stats_table.end(); it++)
	{
		stakes stakes_table(_self, it->token_symbol.raw());
		accounts accounts_table(_self, it->token_symbol.raw());

		eosio::clear_table(stakes_table);
		eosio::clear_table(accounts_table);
	}

	eosio::clear_table(stats_table);
}

//
// Creates or updates a stakable token
//
ACTION atmosstakev2::create(
	eosio::name token_contract,
	eosio::symbol token_symbol,
	eosio::asset round_subsidy,
	int64_t min_claim_secs,
	int64_t min_stake_secs,
	int64_t max_stake_secs,
	eosio::asset min_stake)
{
	eosio::require_auth(_self);

	eosio::check(token_symbol.is_valid(), "invalid token symbol name");
	eosio::check(round_subsidy.is_valid() && round_subsidy.amount > 0 && round_subsidy.symbol == token_symbol, "invalid round subsidy");
	eosio::check(min_stake.is_valid() && min_stake.amount > 0 && min_stake.symbol == token_symbol, "invalid min stake");
	eosio::check(min_claim_secs > 0, "min claim secs must be greater than zero");
	eosio::check(min_stake_secs > 0, "min stake secs must be greater than zero");
	eosio::check(max_stake_secs >= min_stake_secs, "max stake secs must be greater than or equal to min stake secs");

	stats stats_table(_self, _self.value);

	auto stat = stats_table.find(token_symbol.raw());
	if (stat == stats_table.end())
	{
		//
		// Creating a new stakable token
		//
		stats_table.emplace(_self, [&](auto &a) {
			a.total_weight = 0;
			a.total_supply = eosio::asset(0, token_symbol);
			a.subsidy_supply = eosio::asset(0, token_symbol);
			a.round_subsidy = round_subsidy;
			a.token_contract = token_contract;
			a.token_symbol = token_symbol;
			a.last_claim = eosio::current_time_point_sec();
			a.min_claim_secs = min_claim_secs;
			a.min_stake_secs = min_stake_secs;
			a.max_stake_secs = max_stake_secs;
			a.min_stake = min_stake;
		});
	}
	else
	{
		//
		// Updating an existing token
		//
		stats_table.modify(stat, _self, [&](auto &a) {
			eosio::check(a.token_contract == token_contract, "token contract for symbol is not as expected");
			a.round_subsidy = round_subsidy;
			a.min_claim_secs = min_claim_secs;
			a.min_stake_secs = min_stake_secs;
			a.max_stake_secs = max_stake_secs;
			a.min_stake = min_stake;
		});
	}
}

//
// Can only be called by contract itself, used as an emergency exit of all stakes
//
ACTION atmosstakev2::fexitstakes(eosio::symbol token_symbol, eosio::name stakes_to, eosio::name supply_to)
{
	eosio::require_auth(_self);
	stats stats_table(_self, _self.value);
	auto stat = stats_table.find(token_symbol.raw());

	stakes stakes_table(_self, token_symbol.raw());
	accounts accounts_table(_self, token_symbol.raw());

	// eject the stakes
	for (auto it = stakes_table.begin(); it != stakes_table.end(); it++)
	{
		eosio::action(
			permission_level{_self, name("active")},
			stat->token_contract, name("transfer"),
			std::make_tuple(_self, stakes_to, it->balance, eosio::public_key_to_string(it->public_key)))
			.send();
	}

	eosio::clear_table(stakes_table);
	eosio::clear_table(accounts_table);

	// eject the subsidy supply
	if (supply_to != _self && stat->subsidy_supply.amount > 0)
	{
		eosio::action(
			permission_level{_self, name("active")},
			stat->token_contract, name("transfer"),
			std::make_tuple(_self, supply_to, stat->subsidy_supply, "fexitstakes"s))
			.send();
	}

	stats_table.modify(stat, same_payer, [&](auto &a) {
		a.total_supply = eosio::asset(0, token_symbol);
		a.subsidy_supply = eosio::asset(0, token_symbol);
		a.total_weight = 0;
	});
}

//
// Called by a user to exit a stake from the system
// The message below should be signed for the [sig] parameter:
// `atmosstakev2 unstake:${key} ${to} ${memo}`
//
ACTION atmosstakev2::exitstake(uint64_t key, eosio::symbol token_symbol, eosio::name to, string memo, eosio::signature sig)
{
	eosio::check(to != _self, "cannot exit to self");
	eosio::check(token_symbol.is_valid(), "invalid token symbol");

	stakes stakes_table(_self, token_symbol.raw());
	stats stats_table(_self, _self.value);
	accounts accounts_table(_self, token_symbol.raw());
	auto accounts_index = accounts_table.get_index<by_public_key>();

	auto now = eosio::current_time_point_sec();

	auto stake = stakes_table.find(key);
	eosio::check(stake != stakes_table.end(), "stake not found");
	eosio::check(now >= stake->expires, "stake is not yet expired");

	string msg = eosio::format_string("atmosstakev2 unstake:%s %s %s", to_string(key).c_str(), to.to_string().c_str(), memo.c_str());
	eosio::checksum256 digest = eosio::sha256(msg.c_str(), msg.length());
	eosio::assert_recover_key(digest, sig, stake->public_key);

	auto stat = stats_table.find(stake->balance.symbol.raw());
	eosio::check(stat != stats_table.end(), "stat not found");
	eosio::check(stat->total_supply >= stake->balance, "insufficient supply");

	auto account = accounts_index.find(eosio::public_key_to_fixed_bytes(stake->public_key));
	eosio::check(account != accounts_index.end(), "account not found");

	stakes_table.erase(stake);

	stats_table.modify(stat, same_payer, [&](auto &a) {
		a.total_supply -= stake->balance;
		a.total_weight -= stake->weight;
	});

	if (stake->balance.amount == account->total_balance.amount)
	{
		accounts_index.erase(account);
	}
	else
	{
		accounts_index.modify(account, same_payer, [&](auto &a) {
			a.total_balance -= stake->balance;
			a.total_weight -= stake->weight;
		});
	}

	eosio::action(
		permission_level{_self, name("active")},
		stat->token_contract, name("transfer"),
		std::make_tuple(_self, to, stake->balance, memo))
		.send();
}

//
// Admin function for resetting the claim period
//
ACTION atmosstakev2::resetclaim(eosio::symbol token_symbol)
{	
	eosio::require_auth(_self);	
	
	stats stats_table(_self, _self.value);
	auto stat = stats_table.find(token_symbol.raw());

	stats_table.modify(stat, same_payer, [&](auto &a) {
		a.last_claim -= stat->min_claim_secs;
	});
}

//
// Can be called by anyone
// Cycles through all staked amounts for [token_symbol] and awards stake accordingly
// [relay] receives 1% of the [round_subsidy] where as the reminaing 99% is split among stakers
//
ACTION atmosstakev2::claim(eosio::symbol token_symbol, eosio::name relay, string memo)
{
	eosio::check(relay != _self, "self cannot relay");
	eosio::check(token_symbol.is_valid(), "invalid token symbol");

	stakes stakes_table(_self, token_symbol.raw());
	stats stats_table(_self, _self.value);
	accounts accounts_table(_self, token_symbol.raw());
	auto accounts_index = accounts_table.get_index<by_public_key>();

	auto now = eosio::current_time_point_sec();
	auto stat = stats_table.find(token_symbol.raw());
	eosio::check(stat != stats_table.end(), "token not found");

	auto time_delta = eosio::time_diff_secs(now, stat->last_claim);
	eosio::checkf(time_delta >= stat->min_claim_secs, "it has not been a sufficient amount of time since the last claim() call, remaining secs: %d", (stat->min_claim_secs - time_delta));

	eosio::check(stat->subsidy_supply >= stat->round_subsidy, "insufficient subsidy");

	eosio::asset subsidy(stat->round_subsidy.amount * 99 / 100, token_symbol);
	eosio::check(subsidy.is_valid() && stat->round_subsidy > subsidy, "invalid subsidy");

	eosio::asset relay_subsidy(stat->round_subsidy.amount * 1 / 100, token_symbol);
	eosio::check(relay_subsidy.is_valid(), "invalid relay subsidy");
	eosio::check(relay_subsidy.amount > 0, "relay subsidy must be greater than zero, increase relay subsidy by recalling create");

	for (auto stake = stakes_table.begin(); stake != stakes_table.end(); stake++)
	{
		eosio::asset reward(subsidy.amount * (stake->weight) / (stat->total_weight), token_symbol);

		if (reward.amount <= 0 || !reward.is_valid())
			continue; // ignore, insufficient amount

		auto account = accounts_index.find(eosio::public_key_to_fixed_bytes(stake->public_key));
		eosio::check(account != accounts_index.end(), "account not found");

		stakes_table.modify(stake, same_payer, [&](auto &a) {
			a.balance += reward;
		});

		accounts_index.modify(account, same_payer, [&](auto &a) {
			a.total_balance += reward;
		});
	}

	stats_table.modify(stat, same_payer, [&](auto &a) {
		a.subsidy_supply -= stat->round_subsidy;
		a.total_supply += subsidy;
		a.last_claim = now;
	});

	eosio::action(
		permission_level{_self, "active"_n},
		stat->token_contract, "transfer"_n,
		std::make_tuple(_self, relay, relay_subsidy, memo))
		.send();
}

//
// Inline called when receiving a transfer to purpose it to be staked
//
void atmosstakev2::stake(eosio::public_key public_key, eosio::asset balance, eosio::time_point_sec expires)
{
	accounts accounts_table(_self, balance.symbol.raw());
	stakes stakes_table(_self, balance.symbol.raw());
	stats stats_table(_self, _self.value);

	auto stat = stats_table.find(balance.symbol.raw());
	eosio::check(stat != stats_table.end(), "token not found");
	eosio::check(balance >= stat->min_stake, "amount does not meet the minimum stake requirement");

	auto now = eosio::current_time_point_sec();
	eosio::check(expires >= (now + stat->min_stake_secs), "the staking period is too short");
	eosio::check(expires <= (now + stat->max_stake_secs), "the staking period is too long");

	int64_t weight = (balance.amount / 10000) * (eosio::time_diff_secs(expires, now) / 60);
	eosio::check(weight > 0, "weight must be greater than zero");

	stats_table.modify(stat, same_payer, [&](auto &a) {
		a.total_supply += balance;
		a.total_weight += weight;
	});

	stakes_table.emplace(_self, [&](auto &a) {
		a.key = stakes_table.available_primary_key();
		a.weight = weight;
		a.public_key = public_key;
		a.initial_balance = balance;
		a.balance = balance;
		a.expires = expires;
	});

	auto accounts_index = accounts_table.get_index<by_public_key>();
	auto account = accounts_index.find(eosio::public_key_to_fixed_bytes(public_key));
	if (account == accounts_index.end())
	{
		accounts_table.emplace(_self, [&](auto &a) {
			a.key = accounts_table.available_primary_key();
			a.public_key = public_key;
			a.total_balance = balance;
			a.total_weight = weight;
		});
	}
	else
	{
		accounts_index.modify(account, same_payer, [&](auto &a) {
			a.total_balance += balance;
			a.total_weight += weight;
		});
	}
}

//
// Inline called when receiving a transfer which is a subsidy
//
void atmosstakev2::addsubsidy(eosio::asset balance)
{
	stats stats_table(_self, _self.value);

	auto stat = stats_table.find(balance.symbol.raw());
	eosio::check(stat != stats_table.end(), "token not found");

	stats_table.modify(stat, same_payer, [&](auto &a) {
		a.subsidy_supply += balance;
	});
}

//
// Called on an incoming transfer
//
void atmosstakev2::transfer(eosio::name from, eosio::name to, eosio::asset quantity, string memo)
{
	if (from == _self)
		return;

	eosio::check(to == _self, "stop trying to hack the contract");
	eosio::check(quantity.is_valid(), "invalid quantity");
	eosio::check(quantity.amount > 0, "must be positive quantity");

	auto arguments = eosio::split_string(memo, " ");
	eosio::check(arguments.size() > 0, "expected at least one argument");

	stats stats_table(_self, _self.value);
	auto stat = stats_table.find(quantity.symbol.raw());
	eosio::check(stat != stats_table.end(), "token is not supported");

	string method = arguments[0];

	if (method == "stake")
	{
		eosio::check(arguments.size() == 3, "expected exactly 3 arguments");

		auto public_key = eosio::public_key_from_string(arguments[1]);
		auto expires = eosio::current_time_point_sec() + stoi(arguments[2]);

		this->stake(public_key, quantity, expires);
	}
	else if (method == "addsubsidy")
	{
		eosio::check(arguments.size() == 1, "expected exactly 1 argument");

		this->addsubsidy(quantity);
	}
	else
	{
		eosio::check(false, "unknown method");
	}
}

//
// Dispatcher
//
extern "C"
{
	[[eosio::wasm_entry]] void apply(uint64_t receiver, uint64_t code, uint64_t action)
	{
		auto _self = eosio::name(receiver);
		if (code == _self.value)
		{
			//
			// Self dispatched actions (callable contract methods)
			//
			switch (action)
			{
				EOSIO_DISPATCH_HELPER(atmosstakev2, (destroy)(create)(sanity)(exitstake)(fexitstakes)(claim)(resetclaim))
			}
		}
		else
		{
			//
			// If we receive a call from an external contract we care about
			//
			atmosstakev2::stats stats_table(_self, _self.value);
			for (auto it = stats_table.begin(); it != stats_table.end(); it++)
			{
				if (code == it->token_contract.value)
				{
					switch (action)
					{
						EOSIO_DISPATCH_HELPER(atmosstakev2, (transfer))
					}

					break;
				}
			}
		}
	}
}