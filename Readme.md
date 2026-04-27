WELCOME TO PRKL_ULTIMATE (unofficial — based on Spiffy)
=======================================================
This is **prkl_ultimate**, an unofficial fork of [Spiffy_Ultimate](https://github.com/spiffycrew/Spiffy_Ultimate) — which is itself an unofficial fork of Gideon Zweijtzer's [1541ultimate v1.1.0](https://github.com/GideonZ/1541ultimate/commits/1.1.0).

prkl preserves everything Spiffy added (Assembly64 multi-server config, the kickstart RAM-loader, custom branding, dual-joystick menu, hotkeys, flash-protection hardening) and layers a few focused Commodore 64 Ultimate behavior changes on top. The Spiffy material below is unchanged and still describes how those features work.

The project is also known as the "Prkl Patch". ⛧⚡

---

## What prkl changes about the Commodore 64 Ultimate

### USB mouse — high-DPI support
The 1351 mouse interface carries 7-bit quadrature on the SID POT X/Y lines, so any single-poll delta greater than 63 wraps and the C64 reads it as reverse motion. High-DPI USB mice trip this on fast movement.

- **Configurable divisor (1..16, integer)** with fractional remainder, so slow motion at higher divisors isn't truncated away. Higher = slower.
- **Auto-scale**: when |delta| ≥ 40 shows up in three successive USB reports, the divisor doubles (cap 8×); after ~2000 calm packets it steps back down.
- Two new config items in the U64 config menu: **USB Mouse Divisor** and **USB Mouse Auto-Scale**. Defaults preserve legacy behavior — low-DPI mice feel native.

### Web UI additions
Two new pages in the on-device web UI (browse to `http://<your-device>/`):

- **C64 Screen** — pixel-accurate render of the C64 text screen ($0400 + $D800 + $D018) using the genuine Commodore character ROM, embedded in the page as base64. Single snapshot by default; **Refresh** for a fresh frame, **Play / Pause** for live polling at 4 Hz. Auto-detects:
  - Charset 1 vs 2 (uppercase/graphics vs lowercase/uppercase) from `$D018` bit 1.
  - C64 ROM vs U64 II menu font (when the device's own menu is on screen) via a multi-signal heuristic on row 24 (F-key footer), row 1 (banner text) and decoration density. Manual override buttons available.
  Reads pause the 6510 briefly via DMA but don't disturb the active U64 menu (firmware patch — see below). Bitmap modes won't render correctly.
- **Data Streams** — start/stop UDP video/audio/debug streams to a target IP, hits `/v1/streams/<kind>:start` and `:stop`. Defaults to multicast (`239.0.1.64-66`). Each row has an **Open in Player** button that generates a `.m3u` your default player opens; for video the URL uses the `u64://` scheme handled by the [vlc-u64stream](https://github.com/jusii/vlc-u64stream) plugin (drop the `.so` into `~/.local/share/vlc/plugins/` to install). Auto-detects multicast vs unicast in the IP field and emits the right player URL form (`u64://@<group>:port` for multicast, `u64://@:port` for unicast).

#### Firmware fixes that the web UI depends on
Three small patches in `software/io/network/data_streamer.cc` and `software/io/c64/c64_subsys.cc`:

- `ipaddr_aton` is tried before `gethostbyname_r` for stream destinations — lwIP in this build doesn't fall back to `inet_aton` for literal IPs, so plain `192.168.x.y` would fail at the resolve step. Now literal IPs work.
- ARP probing on stream-start uses one persistent UDP socket for all retries (was open/close per probe, which cancelled the pending UDP send and its triggered ARP request before lwIP could resolve). Total ARP budget bumped to ~2.5 s.
- `dma_load_raw_buffer` skips `release_host()` when called for a *read* (rw=1). The original code unconditionally kicked the U64 menu off the C64 every time anything called `readmem`, dropping the user back to BASIC. Reads are now transparent to the menu; writes still take the full release path.

#### Prebuilt binaries
Two prkl-stamped binaries are committed at the repo root for each release so users don't need the Docker build env to test:

- **`Prkl_SoftPatch_<version>.ue2`** — the kickstart RAM-loader. Drop into `/Temp/` on the device, run from the menu (or via [quickstart.sh](quickstart.sh)). No flash writes; power-cycle returns to whatever's flashed.
- **`Prkl_FlashPatch_<version>.ue2`** — programs `ultimate.app` to flash via the kickburn flow. Permanent. **See warning below.**

#### Flashing the FlashPatch — warning
Spiffy's general kickburn warning applies (see [Building / Other Make Targets](#other-make-targets) section below) but a few prkl-specific notes:

- **The kickburn flow only rewrites `ultimate.app`.** It doesn't touch the FPGA bitstream or the `/Flash/` filesystem (HTML, ROMs, carts). So the worst case from a bad prkl ultimate.app is "the firmware doesn't boot" — which the device's recovery / FPGA-side fallback can usually recover from. JTAG-bricking is *very unlikely* unless something has gone catastrophically wrong with the FPGA bitstream itself.
- **Kickburn does not prompt for "Erase Flash?"** That prompt only appears in full `update.ue2` runs that may shuffle FPGA/ROM partitions. If you see it, you're running the wrong binary.
- **Kickburn does not bundle/install HTML.** `SRCS_HTML` is intentionally commented out in `target/u64ii/riscv/kickburn/Makefile`. After flashing, your `/Flash/html/index.html` stays whatever it was — typically whatever the last full `update.ue2` wrote. Use `upload-html.sh` to ship prkl's UI changes alongside.
- **Test SoftPatch first.** Always run a fresh build as kickstart before committing to flash. If the kickstarted version misbehaves, just power-cycle.

#### Updating the on-device HTML
SoftPatch (kickstart) only loads `ultimate.app` into RAM and never touches the flash filesystem. **FlashPatch (kickburn) doesn't touch HTML either** — only the full `update.ue2` does (see the `target/u64ii/riscv/update/Makefile` `SRCS_HTML = index.html` line vs. kickstart/kickburn where it's commented out). So the web UI HTML stays as written by the last full `update.ue2`, plus any `upload-html.sh` overlays.

To deploy `html/index.html` without re-flashing:

    ./upload-html.sh <device-ip-or-hostname>

(default host is `c64u`). The script FTPs the file to `/Flash/html/index.html`. Refresh the browser to see changes. The upload survives reboots, SoftPatches, and FlashPatches; a full `update.ue2` will overwrite it with whatever HTML is bundled in the update binary.

*(More changes will be listed here as the fork grows.)*

---

## License
This project is licensed under the **GNU General Public License v3.0 (GPLv3)**. See [LICENSE](LICENSE.txt).

Copyright © Gideon Zweijtzer, SpiffyCrew et al. See [AUTHORS](AUTHORS.txt).

<i>(To avoid confusion: Gideon is the main author of the original "upstream" project/source tree, but is not involved in this specific "patch".)</i>

---

## Notes on Licensing
On social media we've seen some project-related discussions about the license, potential relicensing, and whether installing modified firmware could or should be prevented.
We'd like to clarify a few points:

### It's GPL
* The original project has been released under the open-source license [GPLv3 by its original creator](LICENSE.txt) (at least since 2011).
* Over the years, [dozens](AUTHORS.txt) of people have contributed. _(The vast majority of the work, of course, was done by Gideon, who deserves almost all the praise for his excellent work and long-term commitment!)_
* The GPL does not distinguish between large and small contributions, nor does it depend on who owns or maintains a repository or who initiated a project.
Every developer owns the copyright for their contributions. Even if it's just a single line of code - you own the copyright for your work.
Relicensing a GPLed project thus requires the explicit permission of *every single* contributor.
Otherwise, the contributions of non-consenting contributors would need to be removed. Neither of these options is practical for an open-source project that has existed for many years - and accepted contributions.
* Even if a project fork was successfully relicensed as closed-source, that would cut it off from any further contributions from the original GPL-licensed project.
What you clearly cannot do, of course, is run an open-source GPL-licensed project as a front end - and then quietly move community contributions from the GPL project into a closed-source back end fork.
* The GPL requires any derivative work to be released under the same license. So, no matter how much a project fork is changed or how much bling is added, it remains under the GPL.

This entire discussion, however, is a storm in a teacup, since the fork in question is in fact published (see [3.14](https://github.com/GideonZ/1541ultimate/commits/v3.14) and [1.1.0](https://github.com/GideonZ/1541ultimate/commits/1.1.0)) including the GPLv3 license attribution - as it needed to.

* By the way, if you’re still unsure about the versions, you can compare the Git commit IDs of the referenced source trees with the "Git hash" shown under "System Information" (when running the official firmware 3.14 or 1.1.0) to confirm that they match the exact source trees used to build those firmware images.

* You can also check their Git history to verify that they are forks of the original open-source project and still include community contributions from many [AUTHORS](AUTHORS.txt).

So, it's GPLv3. Just glad we could clarify this. 😊

### GPLv3 & The Right to Tinker
The (original) project is licensed under GPLv3 - not GPLv2. That may seem like a minor detail. It is not. GPLv3 explicitly added the "_right to tinker_", which GPLv2 lacked.

You can check the details in the [LICENSE](LICENSE.txt), or, read the GNU project's [GPLv3 guide](https://www.gnu.org/licenses/quick-guide-gplv3):

> Some companies have created various different kinds of devices that run GPLed software, and then rigged the hardware so that they can change the software that's running, but you cannot.
> 
> Protecting Your Right to Tinker
> 
> Tivoization is a dangerous attempt to curtail users' freedom: the right to modify your software will become meaningless if none of your computers let you do it.
> GPLv3 stops tivoization by requiring the distributor to provide you with whatever information or data is necessary to install modified software on the device.
> This may be as simple as a set of instructions, or it may include special data such as cryptographic keys or information about how to bypass an integrity check in the hardware.
> It will depend on how the hardware was designed - but no matter what information you need, you must be able to get it.

So, tinkering with a hardware device shipped with firmware based on a GPLv3 sources is legal - and is, in fact, one of the key rights GPLv3 was designed to protect.
That is good news. If you bought a device running GPLv3-based firmware, you can request the vendor to release the source code and allow you to build and install modified firmware (which has worked just fine for us, so we’re good... so far! 😄).
And you have this right - even if a vendor failed to notify you about your rights, as required by the GPL (license information must be embedded in or provided with every shipped unit and software/firmware release).

Again, glad we could clarify this.

### However...
You have the right to tinker, but you also take responsiblity once you modify a device's firmware.
If you break or brick something, don't blame the hardware vendor - nor us, nor anyone else...

---

## Trademarks
The original sources in the _branded fork_ contained certain trademarks and logos.
To avoid potential conflicts, we have removed such trademark references from the source tree, except where fair use applies (when no endorsement is implied, e.g. referring to a specific printer model type).

---

## Custom Branding
We added customizable branding through a separate JSON configuration file.
Separate branding files are a common approach in commercially distributed open-source projects (Linux distros, etc.).
They allow binaries to be built and shipped while keeping protected trademarks separate - outside the GPL-licensed source tree and outside the executables.

You can adjust the title screen - including the logo and colors - to your liking.
If you're using this purely for personal, non-distributed home use, you could theoretically modify the names and logo to restore the exact original appearance. Theoretically.

Here is an example [branding.json](config/branding.json) file. Adjust it to your needs and store it
in the internal flash of your device, at

     /flash/config/branding.json

The `logo/color` array specifies two colors for the two top stripes on the main screen.
Color values correspond to the C64 color palette.

---

## Assembly64 Configuration
The configuration for Assembly64 servers is fully customizable.
You can configure as many servers as you like - even run your own Assembly server at home to access files on your PC from your Ultimate.

Create the following file:

    /flash/config/server.json

This file defines your Assembly servers. It is a simple JSON file (ASCII text).

This example configures only "Assembly64" and shows all available options:

```
{
    "assembly64": [
        {
         "name":         "Assembly64",
         "host":         "hackerswithstyle.se",
         "port":         80,
         "client-id":    "Spiffy",
         "url-search":   "/leet/search/aql/0/100?query=",
         "url-patterns": "/leet/search/aql/presets",
         "url-entries":  "/leet/search/entries",
         "url-download": "/leet/search/bin"
        }
    ]
}
```

Adapt the following example file to your needs and store it on your Ultimate: [server.json](config/server.json).

Note: We also removed the hardware vendor's proprietary server from the source tree. If you bought the commercial variant of the Ultimate (those with white PCBs, shipped in beautiful boxes with manuals, stickers, and everything), you likely have the vendor's permission to download from their site. In that case, you can restore their "Commercial" server address in the configuration file.

### Home Assembly
If you want to run a tiny "Home Assembly" server in your local network, take a look at this simple solution.
It's also useful for testing...

[Spiffy's (Ultimate) Home Assembly 64](https://github.com/spiffycrew/Spiffy_Home_Assembly_64)

## Hotkeys
We’ve added a number of hotkeys to make certain operations more accessible.

*Indeed, we were a bit surprised that the hotkeys received so much attention. The related changes only affect a couple of lines of code - it was really minor work. Sometimes it's the small things that matter...* 😊

### Fixed/Improved Hotkeys
* **C= J**: swap joystick (now also works in the main menu, not just in the file browser)
* **C= HOME** / **HOME**: set home directory / go to home directory (was broken in the commercial fork)
* **C= L**: show developer log (now also works in the main menu, not just in the file browser)

### New Hotkeys
* **C= X**: Reset
* **C= Z**: Reboot
* **C= B**: Power-Cycle
* **C= O**: Power OFF

All hotkeys work only when the menu is active (after pressing C= RESTORE).

## Building
See details in Gideon's original project [README](README.txt) for basics.

We added a new make target:

    make kickstart

This builds a small "kickstart" utility - sometimes also referred to as a "soft patch"... 😆
Its build only requires the rv32 cross-compiler, but not the ESP toolchain etc.
It directly loads and executes the firmware in RAM - and doesn't require any additional hardware. No JTAG (or strings) attached.
It does not touch your flash (original firmware+FPGA), nor does it touch the ESP32 (Wi-Fi & power-controller).

### Kickstart
Build, then copy the ```kickstart.ue2``` to the machine, and execute it.

Kickstart requires matching FPGA and ESP32 firmware to already be present on your board - and it verifies this.
If they do not match, you need to use the normal updater, an official updater, or one from Gideon’s project to install the correct versions.

### Quickstart
* Nifty's tip #1: Use FTP to transfer the kickstarter remotely (e.g., to /Temp).
* Nifty's tip #2: Enable the FAST_LAUNCH switch in the kickstart makefile during development to disable the confirmation dialog.
* Nifty's tip #3: Once confirmation is disabled, you can also launch the kickstarter remotely via Telnet.
* Nifty's tip #4: Use a shell script automating everything. You can then build-run-repeat in seconds - until you mess up and have to power-cycle manually to recover. See [quickstart.sh](quickstart.sh). Just set the target's IP address. Remember to enable Telnet and FTP on your Ultimate.

### Other Make Targets
There are a few more new make targets, i.e.

    make kickburn

This is very similar to "kickstart", but programs flash instead (also known as a "flash patch" 😉). Please be careful! If the firmware you're flashing doesn't work properly, you may end up with a bricked device - and you'll need the right JTAG programmer and cable to recover.

## Shout-out
The Assembly64 team is not involved with this project.
However, if you find Assembly64 useful, please support the guys who maintain the server and provide the storage and bandwidth - to keep their bitstream wiggling and your joysticks waggling.

[assembly64.hackerswithstyle.se](https://assembly64.hackerswithstyle.se)

They also offer Assembly64/Ultimate remote management software for your PC or Mac - which *even runs with stock firmware*! 😃

## FAQ

### Q: Can any of the changes here be integrated into another project, official firmware etc?

A1: Absolutely! All our modifications are released under the GPLv3.
Any project with a matching license is welcome to grab what they like.

A2: There are, however, several features and changes that the other "official" projects may not want.
And that's okay and totally understandable.
So don't expect that certain features added here will soon be integrated elsewhere. They have other commitments and priorities to consider.

### Q: Would you agree to relicense your changes for a closed-source fork?

A: No! 😂

### Q: Is building my own firmware and tinkering with the device safe?

A1: You're on your own. If you're not sure whether you should mess with it, then you probably shouldn't.

A2: With the "kickstarter", running your own firmware is much safer than using a firmware updater, as it doesn't even touch the flash. If you mess up, just power-cycle the system to restore the original state.
The "critical" areas of the flash containing the firmware and FPGA are locked. We actually improved safety by completely removing the code to unlock the flash from the firmware (it was unnecessarily present in the original). Even if a kickstarted firmware image behaved unexpectedly or crashed, the code required to modify the critical areas of the flash memory is not even present at all (it's now only part of the firmware update utility).

### Q: I have an idea or patch. Can you add this?

A: Probably not. We do not plan to maintain this as a "competing fork" or accept pull requests.
We simply added features we personally wanted (and apparently a few others like too :) ).
We released the modified sources as the GPL requires - and as it may help others. If we broke anything with our changes, we will certainly look into it.
We may stumble across something else along the way. But for the moment, we are quite happy with this spiffy retro system.
And we may be swapping the keyboard for a joystick for a while. (Or for a mouse!? 😃)<br>

## Finally...
We'd like to emphasize that Gideon has done excellent work on this project, and his long-term commitment to maintaining it is remarkable. We know from our own experience how exhausting the maintenance of open-source projects can be (lots of requests, demands, and bug reports from random users).

We also know from our (professional) experience that having a good product alone isn't enough. The hardware vendor has done an amazing job here with producing complete units, with beautiful boxes, manuals and everything - and reconnecting with the original spirit.

However, it seems to us that the concept to fork the sources wasn't completely thought through. A fork doesn't change the license, and changing the license would be a bad idea in this case anyway (as the fork would be cut off from contributions from the main GPL-licensed repository). Technically, the fork wasn't really necessary (yes, there are some differences in the hardware (and some GUI differences) compared to Gideon’s original U64EII, but these are marginal compared to the differences in his other hardware variants). All the fork really did was increase the workload, as now two separate source trees need to be maintained. And now there is no easy way for community contributions that are specific to the "branded" variant (and you'll notice that many of our fixes are indeed specific to that variant). So, it seems to us that it would be sensible to revisit the source and fork concept - especially considering that a large number of these devices have been shipped (and the community isn't made up of just gamers - but also of people who love to create and tinker... 😉).

*Dear Ulti-Mates,*<br>
🕹️⌨️ 📺<br>
*Stay spiffy!*
