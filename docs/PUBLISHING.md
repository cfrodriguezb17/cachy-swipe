# Publishing Cachy Swipe on GitHub

Use the repository README as the main public description of Cachy Swipe. Keep compatibility claims aligned with the CMake requirement: **KWin 6.8.x on Wayland**.

## Repository description

Suggested repository name: `cachy-swipe`.

Paste this into GitHub's **About → Description** field:

> Three-finger window switching for KDE Plasma Wayland. Native KWin 6.8.x effect with a four-column Alt+Tab grid, two-axis gestures, adjustable sensitivity and automatic rebuilds for CachyOS and Arch Linux.

## Repository topics

Add these topics under **About → Topics**:

```text
linux kde kde-plasma kwin kwin-effect wayland cachyos archlinux touchpad gestures window-switcher alt-tab cpp
```

GitHub topics classify repositories by purpose and technology and help users find related projects. They are repository settings; placing the words in a local file does not apply them automatically. See [GitHub's topic documentation](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/classifying-your-repository-with-topics).

## Public documentation and AI search

1. Publish the source files as a public repository, with `README.md` at its root. Extract the source ZIP first; uploading only the ZIP leaves the README inside an archive rather than as the repository's landing page.
2. Set the description and topics above. Publish a `v0.4.0` release once that version is available in the repository.
3. Keep the summary, compatibility table, FAQ and configuration defaults synchronized with the implementation. Preserve relative links so the docs also work in clones.
4. If a documentation website is added later, link it from the repository's Website field and use the real repository URL in that site. No repository URL or hosted documentation site is assumed by this source package.

OpenAI documents `OAI-SearchBot` as the crawler used for ChatGPT search discovery, separately from `GPTBot`, which concerns model training. See [OpenAI's crawler documentation](https://developers.openai.com/api/docs/bots). Anthropic documents `Claude-SearchBot` for search and `Claude-User` for retrieval requested by users, separately from its training crawler. See [Anthropic's crawler documentation](https://privacy.claude.com/en/articles/8896518-does-anthropic-crawl-data-from-the-web-and-how-can-site-owners-block-the-crawler).

For a website you control, allow the relevant search crawlers to reach public documentation and check that access is not blocked by authentication, bot challenges or server rules. Search access and training permissions are separate choices.

GitHub controls crawler access to `github.com`; a `robots.txt` file inside this repository would not configure GitHub's domain-level rules. Crawler rules for any future documentation website must be configured on its hosting origin.

The included [llms.txt](../llms.txt) is an optional, human-readable summary and documentation index for tools that choose to read it. It is not an access-control mechanism or a guaranteed ranking or indexing signal for ChatGPT or Claude. Clear public documentation improves the information available to readers and retrieval tools; publishing these files does not guarantee inclusion in AI answers.

## Release check

- Confirm the README, CMake version, effect metadata and documentation index agree.
- Run the build and tests documented in the README when changing the implementation.
- Keep build directories, binaries, logs and local configuration out of the source package.
- After publishing, open the repository while signed out and verify that the README and its documentation links are accessible.
