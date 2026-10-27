# Security

## Reporting a problem

If you find a security problem in Vibebeam, please **do not open a public issue**. Use GitHub's private
reporting instead: open this repository's **Security** tab and choose **Report a vulnerability**. Include
what you did, what you expected and what happened, and the version (web console, Maintenance page).

Fixes are released as a new version and mentioned in the release notes.

## Checking a download

Release files carry a build provenance attestation made by the release workflow. To confirm a file really
came from this repository's workflow, install the GitHub CLI and run:

    gh attestation verify <downloaded file> -R nyteghost/vibebeam

`SHA256SUMS` in each release lists the checksums of the other files.

## Credit

Vibebeam is a fork of [Vibepollo](https://github.com/Nonary/Vibepollo), which builds on Apollo and
Sunshine. Problems that also affect those projects should be reported to them as well.
