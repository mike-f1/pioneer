#ifndef TOMBSTONE_STATE_H
#define TOMBSTONE_STATE_H

#include "PiState.h"

#include <cstdint>
#include <memory>

class Tombstone;

namespace MainState_ {

	class TombstoneState: public PiState {
	public:
		TombstoneState();
		virtual ~TombstoneState();

		PiState *Update() override final;

	private:
		std::unique_ptr<Tombstone> m_cutScene;
		float m_time;
		uint32_t m_last_time;
	};

} // namespace MainState_

#endif // TOMBSTONE_STATE_H
