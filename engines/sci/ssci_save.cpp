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

static unsigned int HANDLE_TYPE_STRING;
static unsigned int HANDLE_TYPE_OBJECT;
static unsigned int HANDLE_TYPE_SCRIPT;
static unsigned int HANDLE_TYPE_VARIABLES;
static unsigned int HANDLE_TYPE_CLASSTBL;
  
SierraSCISaveParser::SierraSCISaveParser(SegManager *s, DisposeAfterUse::Flag dispose) :
	_s(s),
	_dispose(dispose)
{
  
}

void SierraSCISaveParser::dump(Common::MemoryReadStream dump)
{
	int version = dump.readUint32LE();

	switch (version)
	{
	case 20 :
		HANDLE_TYPE_STRING = 24;
		HANDLE_TYPE_OBJECT = 25;
		HANDLE_TYPE_SCRIPT = 37;
		HANDLE_TYPE_VARIABLES = 36;
		HANDLE_TYPE_CLASSTBL = 33;
		
		dump20_21(dump, 20);
		break;
	case 21 :
		HANDLE_TYPE_STRING = 51;
		HANDLE_TYPE_OBJECT = 52;
		HANDLE_TYPE_SCRIPT = 64;
		HANDLE_TYPE_VARIABLES = 63;
		HANDLE_TYPE_CLASSTBL = 60;
		
		dump20_21(dump, 21);
		break;
	default :
		debugN("Can't do textual dump of savegame format %d yet.\n", version);
		break;
	}
}
	
void SierraSCISaveParser::dump20_21(Common::MemoryReadStream &dump, int version)
{
	Common::HashMap<uint16, Common::String> stringMap;
	Common::HashMap<uint16, Object *> scummvmClassMap;
	Common::HashMap<uint16, uint16> varMap;
	byte stringBuffer[500];
	Common::MemoryReadStream object(stringBuffer, 500);	
	int header_size;
	int multidisc_things;
	int disc_number = -1;
	while (dump.readByte());
	if (version == 21)
		disc_number = dump.readUint32LE();
	assert(dump.readUint16LE() == 0x1000);
	debugN("%d\n", dump.pos());	
	dump.skip(version == 21 ? 10 : 8);
	if (disc_number >= 0)
	{
		multidisc_things = dump.readUint16LE();
		dump.skip(2 + 6 * multidisc_things);
	}
	header_size = dump.pos();
	debugN("%d\n", header_size);	

	// First pass: Gather strings
	while (peekUint16LE(dump) != 0)
	{
		uint16 handle = dump.readUint16LE();
		uint32 type = dump.readUint32LE();
		uint32 size = dump.readUint32LE();
		if (version == 21)
			dump.skip(4);
		else
			dump.skip(2);
		debugN("handle %d type %08x size %08x\n", handle, type, size);

		if ((type & 255) == HANDLE_TYPE_STRING)
		{
			if (size <= 500)
			{
				dump.read(stringBuffer, size);
				stringMap[handle] = Common::String((const char *) stringBuffer);
			} else
				dump.skip(size);
		}
		else if ((type & 255) == HANDLE_TYPE_CLASSTBL)
		{
			Common::SeekableReadStream *storedClassTable = dump.readStream(size);
			uint32 cts = storedClassTable->readUint32LE();

			for (uint32 i = 0; i < cts; ++i)
			{
				uint32 class_handle = storedClassTable->readUint32LE();
				uint32 class_script = storedClassTable->readUint32LE();

				if (class_handle != 0)
				{
					if (g_sci->getResMan()->findResource(ResourceId(kResourceTypeScript, class_script), false) != NULL)
					{
						_s->instantiateScript(class_script);
						Object *classRef = _s->getObject(_s->getClass(i).reg);
						if (classRef)
						{
							scummvmClassMap[class_handle] = classRef;
						}
					}  else {	
						debugN("Unable to load class %d", i);
					}
				}
				
			}
			
			delete storedClassTable;
		}
		else if ((type & 255) == HANDLE_TYPE_SCRIPT)
		{
			Common::SeekableReadStream *scriptEntry = dump.readStream(size); 
			scriptEntry->seek(4);
			int scriptId = scriptEntry->readUint16LE();
			scriptEntry->seek(12);
			int varId = scriptEntry->readUint16LE();
			varMap[varId] = scriptId;
			delete scriptEntry;
		} else
		{
			dump.skip(size);
		}
	}

	dump.seek(header_size);
	// Second pass: Parse objects
	while (peekUint16LE(dump) != 0)
	{
		uint16 handle = dump.readUint16LE();
		uint32 type = dump.readUint32LE();
		uint32 size = dump.readUint32LE();
		if (version == 21)
			dump.skip(4);
		else
			dump.skip(2);

		debugN("Memory handle %d of type %d, size %d\n", handle, (type & 255), size);

		if ((type & 255) == HANDLE_TYPE_VARIABLES)
		{
			// This may show one variable too many due to padding, but I don'r care.
			debugN("Variables for script %d:\n\n", varMap[handle]);
			for (uint32 i = 0; i < size/2; ++i)
			{
				int var = dump.readUint16LE();
				debugN("%s_%d = %d (%04x)\n",
				       varMap[handle] == 0 ? "global" : "local",
				       i,
				       var,
				       var);
			}
			debugN("\n");
		}
		else if ((type & 255) == HANDLE_TYPE_OBJECT)
		{
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
					continue;
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
		} else
		{
			dump.skip(size);
		}
	}
}
  
} // end of namespace Sci
