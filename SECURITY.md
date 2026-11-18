# Security policy

## Reporting a vulnerability

Report privately by email to `libraz@libraz.net`, not through the public issue tracker.

Include the input text or file that triggers it, what happened, the affected package and version, your OS, and a minimal reproduction if you have one. Expect an acknowledgement within a few days.

## Supported versions

Pre-1.0. Fixes land on the latest release only; older versions are not patched.

## What is in scope

Suzume tokenizes text it did not produce and loads dictionaries it is handed, so both inputs are the attack surface. In scope:

- Text that causes a crash, a hang, unbounded memory growth, or a read or write outside an allocation, in the C++ core, the CLI or any of the bindings.
- A crafted dictionary file, compiled or a user dictionary source, that does the same, or that makes the tokenizer read a file it was not pointed at.
- Anything that escapes the WebAssembly sandbox, or that lets tokenized input reach the network or the filesystem from the browser build.

## What is not in scope

- Wrong segmentation. A boundary in the wrong place is an accuracy bug; report it as a normal issue.
- Documented limits behaving as documented, such as input length caps. A limit that can be bypassed is in scope.
- Vulnerabilities in the text being tokenized, or in whatever consumes the tokens afterwards.
- Findings that need an attacker to already control the process or to replace the dictionary file on disk.
- Problems in the Go module's own code. Report those to the [go-suzume](https://github.com/libraz/go-suzume) repository; a core vulnerability reached through Go is in scope here.
