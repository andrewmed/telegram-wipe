# Telegram Wipe

Deletes your messages and reactions (AKA likes) for old posts (configurable) in all non-private chats

For those who treat Telegram as a chat and not archival program

## Build
**submodules**
`git sumbodule init`
`git submodule update`

**tdlib**

Run build.sh in top directory

**client**

Descend to `client` dir && run `build.sh`

Builds on Linux and MacOS

## Run
Set up:
`API_ID` env var for telegram key
`API_HASH` env var for telegram token

Descend to `client/build` and run `client` or `run.sh` (to run in docker)

## License
GPLv3




