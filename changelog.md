
## 1.2.1 - 28 Dec 2020

- Branch from sim800c

## 1.2.0 - 30 Aug 2019

- adds support for ESP32
- reads messages faster
- execute all messages in the same loop

## 1.0.2 - 24 Sep 2018

- optimizes code by reusing function
- fixes checking sms possibility of discarding other messages

## 1.0.1 - 12 Sep 2018

- creates a public status() call
- clear connection state when invoking close()
- changes connection state timeout to 10 secs
- creates AT_TERMINATOR as "\r\n"

## 1.0.0 - 5 Sep 2018

- sms: initial support for sms handler, send and remove
- changes debugging to use macro instead of variable
- changes logging to use time instead of millis
- changes loop and fixes some workflow errors
- changes check_command to accept ok/error results with trash next

## 0.1.0 - 28 DEC 2020

- initial commit
