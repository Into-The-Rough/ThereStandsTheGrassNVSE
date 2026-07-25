#pragma once

//runtime vertex shader patching. hooks the grass and speedtree shader load calls, disassembles
//what they loaded, hands the text to whichever feature claims that shader name, reassembles and
//swaps the handle.
//
//the text this passes to a pattern comes from D3DXDisassembleShader, NOT from fxc. the two format
//the same bytecode differently - fxc folds the position staging that the odd speedtree variants
//do through a temp - so patterns must be validated against a DumpDisassembly capture, never
//against fxc output.

namespace ShaderPatch
{
	//returns a malloc'd patched listing, or null to leave the shader vanilla
	typedef char* (*PatchFn)(const char *disassembly);

	//claim is matched against the shader filename, first match wins
	bool RegisterPattern(const char *namePrefix, PatchFn patch);

	bool Install();
	void ReportCounts();

	//writes what d3dx actually produced next to the log, for pattern work
	void SetDumpDisassembly(bool dump);
}
