# macOS signing

The supplied `.scx` files contain both `arm64` and `x86_64` slices. Both slices are
signed with a **local ad-hoc code signature** and verified using:

```sh
codesign --verify --strict --all-architectures --verbose=2 PinkTrombone.scx
codesign --display --verbose=4 PinkTrombone.scx
lipo -archs PinkTrombone.scx
```

Ad-hoc signing establishes the binary's code integrity without an Apple-issued
developer identity. It is appropriate for this locally built, tested extension.
It does **not** establish a Developer ID identity, satisfy Apple notarization, or
guarantee that Gatekeeper accepts a quarantined download on a different Mac.
The build Mac reported **0 valid code-signing identities**, so no Developer ID
or notarization claim is made.

To sign a build using an identity installed in your Keychain:

```sh
SIGN_IDENTITY='Developer ID Application: Your Name (TEAMID)' ./scripts/package_macos.sh
```

That applies the identity, secure timestamp, and hardened-runtime flag to both
plugin binaries before packaging. It does not submit anything to Apple. Developer
ID signing and notarization require your own Apple developer credentials and a
suitable distribution package. The installer deliberately does not alter security
settings or remove quarantine attributes.

The source archive is included so a recipient can build and locally sign their
own copy using `./scripts/build_macos.sh`. Restart both the SuperCollider language
and server after installing, because the language class and server plugin are
loaded separately.

Relevant primary documentation: [Apple code signing](https://developer.apple.com/documentation/security/code-signing-services),
[Apple notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution),
and [SuperCollider UGen loading](https://docs.supercollider.online/Guides/WritingUGens.html).
