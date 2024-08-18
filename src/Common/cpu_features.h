
#ifdef __GNUC__
#define ATTRIBUTE_AVX2 __attribute__((target("avx2")))
#define ATTRIBUTE_SSE41 __attribute__((target("sse4.1")))
#define ATTRIBUTE_AESNI __attribute__((target("aes")))
#else
#define ATTRIBUTE_AVX2
#define ATTRIBUTE_SSE41
#define ATTRIBUTE_AESNI
#endif

class CPUFeaturesImpl
{
public:
	CPUFeaturesImpl();

	std::string GetCPUName(); // empty if not available
	std::string GetCommaSeparatedExtensionList();

	struct
	{
		bool ssse3{ false };
		bool sse4_1{ false };
		bool avx{ false };
		bool avx2{ false };
		bool lzcnt{ false };
		bool movbe{ false };
		bool bmi2{ false };
		bool aesni{ false };
		bool invariant_tsc{ false };
	}x86;
private:
	char m_cpuBrandName[0x40]{ 0 };
};

extern CPUFeaturesImpl g_CPUFeatures;