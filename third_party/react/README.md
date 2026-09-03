# Vendored React

`react.development.js`/`react-dom.development.js` are React's own UMD
builds, unmodified, pinned at **18.3.1** and fetched from
`https://unpkg.com/react@18.3.1/umd/react.development.js` /
`https://unpkg.com/react-dom@18.3.1/umd/react-dom.development.js`.
MIT-licensed (see each file's own header) - not a git submodule since
there's no build step of their own to run, just static text checked in
like any other vendored asset.

`artisan-cli new my-app --lang react` copies these two files, concatenated
in this order, into the new project as `react-runtime.js` - see
README.md's "Using React" section.

To update the pinned version: re-fetch both URLs above with the new
version number and commit the result.
