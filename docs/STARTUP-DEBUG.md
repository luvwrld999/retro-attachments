# Startup diagnostics

The v0.1.2-alpha.1 renderer change did not produce a visible UI on the reported PS5 setup. The diagnostic app emits numbered startup notifications and appends to `/data/retro-attachments-startup.log`. It is an investigation build, not a confirmed fix.

`retro-attachments-startup-debug.elf` is the full viewer with `RETRO_STARTUP_DIAGNOSTICS` defined during the PS5 compile. It also disables SDL framebuffer acceleration before requesting the window surface. No installation occurs.

Stages: 1 main/network initialization; 2 SDL initialization begins; 3 SDL ready; 4 window surface requested; 5 surface ready; 6 renderer ready; 7 font creation; 8 font ready; 9 worker ready; 10 drawing; 11 presentation begins; 12 first frame submitted. An exception is reported as a notification and log entry. A loader failure before main cannot be logged by the app.

Launch through the same Payload Manager method and report the last stage and any error text. The log contains startup markers and errors, not account settings.
