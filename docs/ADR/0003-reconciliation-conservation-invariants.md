# ADR 0003: Population & Financial Conservation Invariants

## Context
Financial platforms must provide mathematical guarantees that no transaction or monetary amount disappears silently during processing or routing.

## Decision
Implemented strict double-entry population and financial conservation tracking:
1. $\sum 	ext{states} = 	ext{input count}$
2. $\sum 	ext{micros} = 	ext{input micros}$
Any discrepancy aborts the batch with `SEV_FATAL`.

## Status
Accepted and Implemented.
