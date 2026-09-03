<p align="center">
    <a href="https://github.com/xenia-canary/xenia-canary/tree/canary_experimental/assets/icon">
        <img height="256px" src="https://raw.githubusercontent.com/xenia-canary/xenia/master/assets/icon/256.png" />
    </a>
</p>

<h1 align="center">Xenia Edge - Xbox 360 Emulator</h1>

Xenia Edge is yet another experimental fork of the Xenia emulator, originally based on [Xenia Canary](https://github.com/xenia-canary/xenia-canary). The focus is
on faster iteration, higher default game compatibility, usability and platform support.

## Status

Build (Windows / Linux / macOS arm64 / macOS x86_64): [![CI](https://github.com/has207/xenia-edge/actions/workflows/CI.yml/badge.svg?branch=edge)](https://github.com/has207/xenia-edge/actions/workflows/CI.yml) [![Codacy Badge](https://app.codacy.com/project/badge/Grade/cd506034fd8148309a45034925648499)](https://app.codacy.com/gh/has207/xenia-edge/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)

Releases
--------
[Latest](https://github.com/has207/xenia-edge/releases/latest) ◦ [All](https://github.com/has207/xenia-edge/releases)

FAQ
---

- Q: Why is game X running slower on Xenia Edge than Xenia Canary?<br>
  A: Edge defaults to accuracy over performance. You can tune this per-game by using the Guide button while in-game (PS or Xbox button in the middle of the controller) and going to Performance, then disabling various options there to reach the same performance as Canary.

- Q: How to tell what options a particular game might need?<br>
  A: Many games that are not running well by default require one or two simple config changes that make them (near) perfect. To find out, right-click the game in the game list and go to Compatibility. If there is an existing compatibility page there will be a link to Master, Caanary, or both. Prefer information on the Canary page, you will often find the answer there.

- Q: Why are translations into langauage X so bad?<br>
  A: They're AI-generated, if you speak the language and want to help edit the relevant .po file in the assets directory and submit it. If you don't know how to use git then just open a bug and attach the fixed .po file. Best way to edit those is with a program called Poedit.

- Q: Should I use Vulkan on Windows?<br>
  A: Probably not, while vulkan is now on par with d3d12 in terms of graphical fidelity it's still less performant.

- Q: Should I use the Windows build with wine/proton on Linux?<br>
  A: While the answer has been "yes" for years it is no longer the case. The native build for Linux is quite competitive with Windows now and is recommened over wine/proton.


