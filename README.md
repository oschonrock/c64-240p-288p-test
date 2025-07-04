This is a "240/288p test for the C64". Modelled on the "SNES
240p test suite", specifically the "drop shadow test" and the "striped
sprite test".

Just directly run the `.prg` from the latest release, Alternatively, 
to compile, download the oscar64 compiler, compile it, then clone this
repo and run `make`.

Run the resulting binary on your C64 with LCD/CRT attached and give
feedback on what you see / how the programme could be more useful. Or
what LCDs exhibit weird behaviour.

The point here is: CRTs are our reference case, this is how the C64 is
supposed to work. We want LCDs that get close to that experience
without introducing weird artifacts because they don't understand the
C64 signal. This programme is meant to help selecting LCDs that work
well with the C64 signal, by providing an easy objective test.

Usage:

Use Joystick in Port 2 to move shape around.

F1 - (default) "Drop shadow test" - actually I think "flicker test"
test is a better name. Shows a black shape sprite on a grey bakcground
and that sprite is hidden every alternate frame - ie shown in "odd
field" and hidden in "even field". Except of course the C64 doesn't
have odd/even fields because it renders "50/60 progressive frames".
Many LCDs are confused by this and will try to "deinterlace the
odd/even fields" that don't exist.

Good outcome: The Sprite flickers at 25Hz => display understands
progressive 240/288p

Bad Outcomes: a) "solid grey" shape b) Stripey shape c) stripey and
                              weird artifacts when moved

F3 - "Striped Sprite Test". The same shape with alternate pixel rows
deleted on top of a stripey background.

Good outcome: A Solid black shape where the missing rows of pixels are
filled in by the background. Or after moving it to an even row,
"nothing at all" , because the sprite pixels match the backrground.

Bad Outcome: Not sure haven't seen one

