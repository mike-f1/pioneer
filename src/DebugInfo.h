#ifndef DEBUGINFO_H
#define DEBUGINFO_H

#include <cstdint>
#include <string>

class DebugInfo final
{
public:
	DebugInfo() = default;
	~DebugInfo() = default;
	DebugInfo(const DebugInfo& rhs) = delete;
	DebugInfo& operator=(const DebugInfo& rhs) = delete;

	void NewCycle();
	void IncreaseFrame() { m_frame_stat++; };
	void IncreasePhys(int delta) { m_phys_stat += delta; };

	void Update();
	void Print();

private:
	int m_frame_stat;
	int m_phys_stat;
	uint32_t m_last_stats;

	std::string m_dbg_text;
};

#endif // DEBUGINFO_H
