# Mastodon Toot

A simple module, that shares posts on a Mastodon instance.

It creates a remote toot, whenever a node is published (newly created,
manually published or scheduled).
It deletes the remote toot, whenever a node is deleted
 or unpublished.

It will not update remote toots.

**Note**: This module is in an early stage of development, bigger changes are likely.

## Installation

- Install this module using the official 
  [Backdrop CMS instructions](https://docs.backdropcms.org/documentation/extend-with-modules)

You'll need to create an access token on your Mastodon instance. Go to your
account's preferences, switch to "Development" in the left sidebar, and add an
 "App" with the "New application" button.

"Application name" can be anything (it will get displayed under the toot,
next to the date).

The "Scopes" are important, check:

- read:accounts
- read:statuses
- write:statuses

Save settings, open again and copy "Your access token".
Caution: every time you save this application form, a new token is generated. Don't
forget to copy new tokens over to your Backdrop site.

Switch to Backdrop, logged in as admin, go to "Configuration / Web services / Mastodon Toot".
Insert your access token there, also set the instance URL and choose a
content type. Save configuration.

Now switch to the "Verify credentials" tab, and hit the "Check" button. It
should show a friendly green message.

## Issues

Bugs and Feature requests should be reported in the 
 [Issue Queue](https://github.com/backdrop-contrib/mastotoot/issues)

## Current Maintainers

- [Indigoxela](https://github.com/indigoxela)

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
