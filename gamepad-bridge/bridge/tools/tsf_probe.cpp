// tsf-probe: read libdrc's GetTsf a few times (honours DRC_TSF_SOURCE, libdrc patch 0005), print values and deltas.
// usage: [DRC_TSF_SOURCE=...] tsf-probe
#include <drc/internal/tsf.h>

#include <cstdio>
#include <unistd.h>

int main()
{
	drc::u64 prev = 0;
	int failures = 0;
	for (int i = 0; i < 5; i++)
	{
		drc::u64 t = 0;
		const int rv = drc::GetTsf(&t);
		if (rv != 0)
			failures++;
		printf("GetTsf rv=%d value=%llu", rv, (unsigned long long)t);
		if (i)
			printf("  (+%lld us)", (long long)(t - prev));
		printf("\n");
		prev = t;
		usleep(200000);
	}
	return failures ? 1 : 0;
}
