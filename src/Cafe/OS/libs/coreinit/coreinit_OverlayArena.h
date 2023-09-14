namespace coreinit
{
	void ci_OverlayArena_Save(MemStreamWriter& s);
	void ci_OverlayArena_Restore(MemStreamReader& s);

	void InitializeOverlayArena();
};