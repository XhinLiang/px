## Technical details

- In my MacBook(Intel) **sometimes** (siginfo_t *info) will give me 0 pid which I don't know why?
  - Refs
    - https://developer.apple.com/forums/thread/721195
    - https://stackoverflow.com/questions/71132499/siginfos-si-pid-sets-itself-to-0-after-a-few-calls-of-the-same-function
  - Solution
    - In exchange, when I got 0 pid in the signal, I will try to read all of the traders' pipes try to figure out which trader send the message
    - In trader, when I got 0 pid in the signal, I will use the stored pid in the trader process which will be passed in the command line params
- I designed two types of trader, they are based on the same codebase pe_trader_common.c
  - pe_trader.c is the auto trader which the assigment metion, it will place the BUY order according to the SELL message of exchange
  - pe_trader_seller.c is a auto sell trader, it will send the SELL message when it receive message from exchange
- I used mutex in exchange and trader, both for ensure that there will only be one signal processe in a same time
- I used another single thread(pthread) in exchange
  - For checking if there are some trader processes exit, and then mark it as disconnected
  - If all of the trader processes exited, exchange will print the stat summary and then exit itself

## Unresolved problems

- Sometimes the exchange will exit itself before print the stat summary
  - I don't know why, maybe it's a segment fault??