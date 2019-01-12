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
#include <sci/ssci_save.h>
#include <sci/engine/kernel.h>

namespace Sci {

SierraSCISaveParser::SierraSCISaveParser(SegManager *s, DisposeAfterUse::Flag dispose) :
	_s(s),
	_dispose(dispose)
{
  
}
  
void SierraSCISaveParser::dump(Common::MemoryReadStream dump)
{
	Common::HashMap<uint16, Common::String> stringMap;
	Common::HashMap<uint16, Object *> scummvmClassMap;
	Common::HashMap<uint16, uint16> varMap;
	byte stringBuffer[500];
	Common::MemoryReadStream object(stringBuffer, 500);	
	dump.skip(21);
	
	// First pass: Gather strings
	while (peekUint16LE(dump) != 0)
	{
		uint16 handle = dump.readUint16LE();
		uint32 type = dump.readUint32LE();
		uint32 size = dump.readUint32LE();
		dump.skip(4);

		switch (type & 255)
		{
		case 51 : // String
			if (size <= 500)
			{
				dump.read(stringBuffer, size);
				stringMap[handle] = Common::String((const char *) stringBuffer);
			} else
				dump.skip(size);
			break;
		case 60 : // Class table
		{
			Common::SeekableReadStream *storedClassTable = dump.readStream(size);
			uint32 cts = storedClassTable->readUint32LE();
			if (cts != _s->classTableSize()-1)
			{
				debugN("Class table size in savegame differs from the running game: %d != %d!\n",
				       cts, _s->classTableSize());
				delete storedClassTable;
				return;
			}

			for (uint32 i = 0; i < cts; ++i)
			{
				uint32 class_handle = storedClassTable->readUint32LE();
				uint32 class_script = storedClassTable->readUint32LE();

				if (class_handle != 0)
				{
					_s->instantiateScript(class_script);
					Object *classRef = _s->getObject(_s->getClass(i).reg);
					if (classRef)
					{
						scummvmClassMap[class_handle] = classRef;
					} else {	
						debugN("Unable to load class %d", i);
					}
				}
				
			}
			
			delete storedClassTable;
			break;
		}
		case 64 : // Script
		{
			Common::SeekableReadStream *scriptEntry = dump.readStream(40);
			scriptEntry->seek(4);
			int scriptId = scriptEntry->readUint16LE();
			scriptEntry->seek(12);
			int varId = scriptEntry->readUint16LE();
			varMap[varId] = scriptId;
			delete scriptEntry;
			break;
		}
		default :
			dump.skip(size);
		}
	}

	dump.seek(21);
	// Second pass: Parse objects
	while (peekUint16LE(dump) != 0)
	{
		uint16 handle = dump.readUint16LE();
		uint32 type = dump.readUint32LE();
		uint32 size = dump.readUint32LE();
		dump.skip(4);

		debugN("Memory handle %d of type %d, size %d\n", handle, type & 255, size);
		switch (type & 255)
		{
		case 63 : // Variables block
		{
			// This may show one variable too many due to padding, but I don'r care.
			debugN("Variables for script %d:\n\n", varMap[handle]);
			for (uint32 i = 0; i < size/2; ++i)
			{
				int var = dump.readUint16LE();
				debugN("%s_%d = %04x (%d)\n",
				       varMap[handle] == 0 ? "global" : "local",
				       i,
				       var,
				       var);
			}
			debugN("\n");
			break;
		}
		case 52 : // Object
			if (size <= 500)
			{
				int classScript, super, info, name;
				Object *theClass;
				
				dump.read(stringBuffer, size);
				object.seek(0);
				object.skip(8);
				classScript = object.readUint16LE();
				object.skip(2);
				super = object.readUint16LE();
				info = object.readUint16LE();
				name = object.readUint16LE();

				if (info & kInfoFlagClass)
				{
					theClass = _s->getObject(_s->getClass(classScript).reg);
				}
				else
				{
					theClass = scummvmClassMap[super];
				}

				if (!theClass)
				{
					debugN("Failed to get class for handle %d (%04x, %04x)\n", handle, info, classScript);
				}

				object.seek(0);
				debugN("[%d] %s %s :  %d vars\n",
				       handle,
				       info & kInfoFlagClass ? "class" : "object",
				       stringMap[name].c_str(), theClass->getVarCount());
				if (!(info & kInfoFlagClass) )
					debugN("Superclass is %s\n\n", _s->getString(theClass->getNameSelector()).c_str());
				else
					debugN("\n");
				for (uint32 i = 0; i < theClass->getVarCount(); ++i)
				{

					int propval = object.readUint16LE();
					
					debugN("%s = %04x (%d)\n",
					       g_sci->getKernel()->getSelectorName(theClass->getVarSelector(i)).c_str(),
					       propval,
					       propval);
				}
				debugN("\n");
			} else
				dump.skip(size);
			break;
		default :
			dump.skip(size);
		}
	}
	
}
  
} // end of namespace Sci
