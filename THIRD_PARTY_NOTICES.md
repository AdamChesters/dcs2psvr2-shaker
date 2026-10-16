# Credits and source provenance

dcs2psvr2-shaker is developed by Adam Chesters. Project code is GPL-3.0; third-party components retain their original licenses.

- **Bnuuy Solutions / PSVR2Toolkit**, MIT, revision `629a224a7a63d2707935e19fb575d8f182305d7d`: headset C API definitions and device behavior reference. https://github.com/BnuuySolutions/PSVR2Toolkit
- **Tabitha Moon / PSVR2HeadpatHaptics**, MIT, revision `307c7ddcf3b09ef348e949b2a15144d216234264`: inspiration for normalized-input-to-rumble mapping. No legacy C# binding is used. https://github.com/tabithamoon/PSVR2HeadpatHaptics
- **Valmantas Paliksa, Micah Frisby and TelemFFB contributors**, GPL-3.0, revision `e4e4f2f4beecf8fd749ab567831a0db319a32980`: DCS export signal, aircraft draw-argument and Apache rotor-parameter references adapted into our independent exporter. https://github.com/walmis/VPforce-TelemFFB
- **Adam Chesters / PSVR2 Passthrough Layer contributors**, MIT: ImGui/Win32/D3D11 application bootstrap and GUI inspiration. https://github.com/AdamChesters/psvr2passthrough
- **Omar Cornut and Dear ImGui contributors**, MIT: GUI and Win32/D3D11 backends, version pinned by vcpkg. https://github.com/ocornut/imgui
- **Niels Lohmann and JSON for Modern C++ contributors**, MIT: JSON serialization, version pinned by vcpkg. https://github.com/nlohmann/json

PSVR2Toolkit is a separately installed dependency; no Sony firmware or driver binaries are distributed.
