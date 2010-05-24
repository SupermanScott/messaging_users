Messaging Users module: README.txt
==================================
This module allows sending private messages to users and being notified by other method of received messages.
I.e. you can get a daily digest with private messages sent to you.

Required modules
----------------
- Messaging and some sending method enabled
- Messating notifications.
- Notifications
- Flag and Flag friend

Basic set up
-------------
Once enabled all the required modules, there are some administration pages to check and set up options

General settings for messaging and notifications
- Messaging & Notifications > Messaging Settings
- Messaging & Notifications > Notifications settings

Spam control and Flag integration
- Messaging & Notifications > Messaging Settings > User messages

There are spam controls here that limit the number of messages to non friends per day.
A limit can be set up for each role, the user will be getting the maximum from all his roles.

Note: Zero (0) means no limit, so if the user has a role with that, he'll be able to send any number of messages.


Set up the 'Ignore' Flag (A user flag that must be created previously) to flag users you don't want to get messages from.
Flagging a user will cause messages to be logged (for spam control and sender outbox) but not showing up on the Inbox.  

Permissions: Users > Permissions
--------------------------------
There are three permissions created by this module:
- send messages to friends (Only messages to friends)
- send messages to users (Send messages to any user)
- send messages to all users (Enables bulk messages to 'All users' or to users with a given role)

Basic usage
-----------
- Under the own user account there's a 'Messages' tab with Inbox/Outbox/Compose subtabs.
- For each other user there's a 'Send message' link.
