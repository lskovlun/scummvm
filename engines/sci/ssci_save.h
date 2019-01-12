/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <sci/engine/seg_manager.h>

namespace Sci {
  
class SierraSCISaveParser {
public:
	SierraSCISaveParser(SegManager *s, DisposeAfterUse::Flag dispose = DisposeAfterUse::NO);

	void dump(Common::MemoryReadStream data);
	~SierraSCISaveParser() {
			if (_dispose == DisposeAfterUse::YES)
				delete _s;
		}
private:
	SegManager *_s;
	DisposeAfterUse::Flag _dispose;
	void dump20_21(Common::MemoryReadStream &data, int version);
	uint16 peekUint16LE(Common::MemoryReadStream &data) {
		int32 pos = data.pos();
		uint16 ret = data.readUint16LE();
		data.seek(pos);
		return ret;
	}
};
  
}
