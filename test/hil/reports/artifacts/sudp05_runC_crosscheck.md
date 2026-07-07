# S-UDP-05 UART/UDP cross-check

- UART dumps with state: 48
- UDP rows: 23558
- UDP stall markers (missed_periods >= 10): 47

## State agreement (criterion 1, state fields)

| # | UART time | UART state | nearest UDP state | dt (s) | verdict |
|---|---|---|---|---|---|
| 0 | 09:59:37 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 1 | 09:59:42 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 2 | 09:59:48 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | -0.1 | MATCH |
| 3 | 09:59:53 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 4 | 09:59:58 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 5 | 10:00:03 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 6 | 10:00:08 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 7 | 10:00:13 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 8 | 10:00:18 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 9 | 10:00:23 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 10 | 10:00:28 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 11 | 10:00:33 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 12 | 10:00:39 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | -0.1 | MATCH |
| 13 | 10:00:44 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 14 | 10:00:49 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 15 | 10:00:54 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 16 | 10:00:59 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 17 | 10:01:04 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 18 | 10:01:09 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 19 | 10:01:14 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 20 | 10:01:19 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 21 | 10:01:24 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 22 | 10:01:30 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | -0.1 | MATCH |
| 23 | 10:01:35 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 24 | 10:01:40 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 25 | 10:01:45 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 26 | 10:01:50 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 27 | 10:01:55 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 28 | 10:02:00 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 29 | 10:02:05 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 30 | 10:02:10 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 31 | 10:02:15 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 32 | 10:02:21 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | -0.1 | MATCH |
| 33 | 10:02:26 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 34 | 10:02:31 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 35 | 10:02:36 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 36 | 10:02:41 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 37 | 10:02:46 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 38 | 10:02:51 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 39 | 10:02:56 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 40 | 10:03:01 | CVC=OK FZC=OK RZC=OK relay=ON | health=0x7 relay=1 reason=NONE | +0.0 | MATCH |
| 41 | 10:03:06 | CVC=TIMEOUT FZC=OK RZC=TIMEOUT relay=ON | health=0x2 relay=1 reason=NONE | +0.0 | MATCH |
| 42 | 10:03:12 | CVC=TIMEOUT FZC=OK RZC=TIMEOUT relay=ON | health=0x2 relay=1 reason=NONE | -0.0 | MATCH |
| 43 | 10:03:17 | CVC=TIMEOUT FZC=OK RZC=TIMEOUT relay=ON | health=0x2 relay=1 reason=NONE | +0.0 | MATCH |
| 44 | 10:03:22 | CVC=TIMEOUT FZC=OK RZC=TIMEOUT relay=ON | health=0x2 relay=1 reason=NONE | +0.0 | MATCH |
| 45 | 10:03:27 | CVC=TIMEOUT FZC=OK RZC=TIMEOUT relay=ON | health=0x2 relay=1 reason=NONE | +0.0 | MATCH |
| 46 | 10:03:32 | CVC=TIMEOUT FZC=OK RZC=TIMEOUT relay=ON | health=0x2 relay=1 reason=NONE | +0.0 | MATCH |
| 47 | 10:03:37 | CVC=TIMEOUT FZC=OK RZC=TIMEOUT relay=ON | health=0x2 relay=1 reason=NONE | -3.8 | MATCH |

## Counter identities (criterion 1, counters)

| interval | UDP frames | missed | frames+missed (exp 500) | d(tx7 call) (exp 50) | d(ok) | d(fail) | verdict |
|---|---|---|---|---|---|---|---|
| 0 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 1 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 2 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 3 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 4 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 5 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 6 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 7 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 8 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 9 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 10 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 11 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 12 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 13 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 14 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 15 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 16 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 17 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 18 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 19 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 20 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 21 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 22 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 23 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 24 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 25 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 26 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 27 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 28 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 29 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 30 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 31 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 32 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 33 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 34 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 35 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 36 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 37 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 38 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 39 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 40 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 41 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 42 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 43 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 44 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |
| 45 | 500 | 0 | 500 | 50 | 50 | 0 | PASS |

## Alignment sanity

- stall/dump pairs matched: 47; unmatched stalls: 0; worst matched clock skew: 0.97 s (limit 2.0 s)

**Result: PASS** (0 failing check(s))
