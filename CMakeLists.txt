cmake_minimum_required(VERSION 3.5)
project(atmosstakev2_example VERSION 1.0.0)

find_package(eosio.cdt)

### Generate the wasm and abi
add_contract( atmosstakev2 atmosstakev2 atmosstakev2.cpp base58.cpp )
