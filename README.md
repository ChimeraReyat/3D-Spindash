# 3D-Spindash
Makes 3D spindash in Generations not crash, with added bonus of changing your rotation while charging and B-Button roll

This is a mod designed to give Classic Sonic a workable spindash in 3D, without crashing--with a bonus ability to change direction while charging a la Sonic Adventure, as well as entering roll while pressing B. 

For the curious, the crash was avoided by setting his spin function to use his "Slide" function instead. Turns out, Classic has one! There's probably better ways to go about this, but this is good enough to just get it working.

Some caveats:

- His spin parameters don't actually affect him at all when rolling in 3D, but they do in 2D, which is a bit of an inconsistency.
- Slide parameters don't seem to affect him *either,* meaning the only things that affect him when rolling are standard physics params.

By design, this is very bare-bones, not adding anything else fancy. This means no spin-cancelling/spamdash, or a good turning radius--you really can't turn much at all honestly. However, this is provided with the full source code, so feel free to modify it however you see fit. Though, if you don't know any assembly... good luck, haha.
