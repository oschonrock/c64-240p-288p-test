# always keep the .prg, even if subsequent tasks (like running vice) "fails" or in innterrupted
.PRECIOUS: %.prg

FORCE:

%.prg: %.cpp FORCE
	~/c64/oscar64/bin/oscar64 -pp -g -xz -Oz -Ox -O2 -dNOFLOAT -DNDEBUG -DDBGMSG $<

%.run: %.prg FORCE
	x64sc -autostartprgmode 1 -autostart-warp +cart -moncommands $*.lbl -nativemonitor --silent $< 

%.prod: %.cpp FORCE
	~/c64/oscar64/bin/oscar64 -pp -g -dNOLONG -dNOFLOAT -DNDEBUG -O2 -Ox $<

%.debprg: %.cpp FORCE
	~/c64/oscar64/bin/oscar64 -pp -g -dNOLONG -dNOFLOAT -dCAPACITYCHECK -O2 -Ox $<

%.deb: %.debprg FORCE
	x64sc -autostartprgmode 1 -autostart-warp +cart -moncommands $*.lbl -nativemonitor --silent $*.prg

%.pro: %.cpp FORCE
	~/c64/oscar64/bin/oscar64 -pp -ep -gp -dNOLONG -dNOFLOAT -DNDEBUG -DPROFILE -O2 -xz -Oz -Ox $<


clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs *.dbj *.csz
