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

#include "trecision/trecision.h"
#include "trecision/nl/3d/3dinc.h"
#include "trecision/nl/define.h"
#include "trecision/nl/extern.h"
#include "trecision/nl/sysdef.h"
#include "trecision/nl/message.h"
#include "trecision/graphics.h"
#include "trecision/video.h"

#include "engines/util.h"
#include "common/scummsys.h"
#include "common/error.h"
#include "common/system.h"
#include "common/events.h"
#include "common/archive.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/fs.h"
#include "logic.h"
#include "graphics/cursorman.h"

namespace Common {
class File;
}

namespace Trecision {

extern byte NlVer;

TrecisionEngine *g_vm;

TrecisionEngine::TrecisionEngine(OSystem *syst) : Engine(syst) {
	const Common::FSNode gameDataDir(ConfMan.get("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "AUTORUN");
	SearchMan.addSubDirectoryMatching(gameDataDir, "DATA");
	SearchMan.addSubDirectoryMatching(gameDataDir, "FMV");

	g_vm = nullptr;

	_curRoom = 0;
	_oldRoom = 0;
	_curInventory = 0;
	_curSortTableNum = 0;

	for (int i = 0; i < 10; ++i)
		_curScriptFrame[i] = 0;

	// Inventory
	for (int i = 0; i < MAXICON; ++i) {
		_inventory[i] = 0;
		_cyberInventory[i] = 0;
	}

	_inventorySize = 0;
	_cyberInventorySize = 0;
	_iconBase = 0;
	_inventoryRefreshStartIcon = 0;
	_lastCurInventory = 0;
	_flagInventoryLocked = false;
	_inventorySpeed[0] = 20;
	_inventorySpeed[1] = 10;
	_inventorySpeed[2] = 5;
	_inventorySpeed[3] = 3;
	_inventorySpeed[4] = 2;
	_inventorySpeed[5] = _inventorySpeed[6] = _inventorySpeed[7] = 0;
	_inventorySpeedIndex = 0;
	_inventoryScrollTime = 0;

	_fastWalk = false;
	_fastWalkLocked = false;
	_mouseONOFF = true;

	// Use With
	_useWith[0] = _useWith[1] = 0;
	_useWithInv[0] = _useWithInv[1] = false;

	// Messages
	for (int i = 0; i < MAXOBJNAME; ++i)
		_objName[i] = nullptr;

	for (int i = 0; i < MAXSENTENCE; ++i)
		_sentence[i] = nullptr;

	for (int i = 0; i < MAXSYSTEXT; ++i)
		_sysText[i] = nullptr;

	_curMessage = nullptr;

	// Original values
	_idleMsg = {MC_IDLE, 0, MP_DEFAULT, 0, 0, 0, 0, 0};
	_curObj = 1;
	_inventoryStatus = INV_OFF;
	_lightIcon = 0xFF;
	_inventoryRefreshStartLine = INVENTORY_HIDE;
	_lastLightIcon = 0xFF;
	_inventoryCounter = INVENTORY_HIDE;

	_screenBuffer = nullptr;
	_animMgr = nullptr;
	_graphicsMgr = nullptr;
	_logicMgr = nullptr;

	for (int i = 0; i < 50; ++i) {
		for (int j = 0; j < 4; ++j) {
			_limits[i][j] = 0;
		}
	}

	_limitsNum = 0;
	_actorLimit = 0;
	_nextRefresh = 0;

	_curKey = Common::KEYCODE_INVALID;
	_curAscii = 0;
	_mouseX = _mouseY = 0;
	_mouseLeftBtn = _mouseRightBtn = false;
	_oldMouseX = _oldMouseY = 0;
	_keybInput = false;

	_gamePaused = false;
	_flagMouseEnabled = true;

	_closeUpObj = 0;
	_textPtr = nullptr;
	lastinv = 0;
	lastobj = 0;

	_slotMachine41Counter = 0;

	_curStack = 0;
	_textStackTop = -1;
	
	_flagscriptactive = false;
	_flagScreenRefreshed = false;

	_wheel = 0xFFFF;
	for (int i = 0; i < 3; ++i)
		_wheelPos[i] = 0;

	Font = nullptr;
	Arrows = nullptr;
	TextureArea = nullptr;
	Icone = nullptr;
	ZBuffer = nullptr;
	_actor = nullptr;
}

TrecisionEngine::~TrecisionEngine() {
	delete _animMgr;
	delete _graphicsMgr;
	delete _logicMgr;
	delete[] Font;
	delete[] Arrows;
	delete[] TextureArea;
	delete[] Icone;
	delete[] ZBuffer;
	delete _actor;
}

Common::Error TrecisionEngine::run() {
	g_vm = this;

	syncSoundSettings();
	
	_graphicsMgr = new GraphicsManager(this);
	if (!_graphicsMgr->initScreen())
		return Common::kUnsupportedColorMode;
	_animMgr = new AnimManager(this);
	_logicMgr = new LogicManager(this);

	initMain();
	initCursor();
	
	while (!g_engine->shouldQuit()) {
		eventLoop();
		NextMessage();
	}

	return Common::kNoError;
}

void TrecisionEngine::eventLoop() {
	Common::Event event;
	while (g_system->getEventManager()->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_MOUSEMOVE:
			_mouseX = event.mouse.x;
			_mouseY = event.mouse.y;
			break;

		case Common::EVENT_LBUTTONDOWN:
			_mouseLeftBtn = true;
			break;

		case Common::EVENT_LBUTTONUP:
			_mouseLeftBtn = false;
			break;

		case Common::EVENT_RBUTTONDOWN:
			_mouseRightBtn = true;
			break;

		case Common::EVENT_RBUTTONUP:
			_mouseRightBtn = false;
			break;

		case Common::EVENT_KEYDOWN:
			if (event.kbd.keycode == Common::KEYCODE_CAPSLOCK) {
				if (!_fastWalkLocked)
					_fastWalk ^= true;
				_fastWalkLocked = true;
			}
			break;

		case Common::EVENT_KEYUP:
			_curKey = event.kbd.keycode;
			_curAscii = event.kbd.ascii;
			switch (event.kbd.keycode) {
			case Common::KEYCODE_p:
				if (!_gamePaused && !_keybInput) {
					_curKey = Common::KEYCODE_INVALID;
					_gamePaused = true;
					waitKey();
				}
				_gamePaused = false;
				break;

			case Common::KEYCODE_CAPSLOCK:
				_fastWalkLocked = false;
				break;
			default:
				break;
			}
			break;

		default:
			break;
		}
	}
	g_system->delayMillis(10);
	g_system->updateScreen();
}

bool TrecisionEngine::hasFeature(EngineFeature f) const {
	return (f == kSupportsLoadingDuringRuntime) ||
		   (f == kSupportsSavingDuringRuntime);
}

Common::Error TrecisionEngine::loadGameStream(Common::SeekableReadStream *stream) {
	byte version = stream->readByte();
	// TODO: Check for newer save versions
	Common::Serializer ser(stream, nullptr);
	ser.setVersion(version);
	syncGameStream(ser);
	return Common::kNoError;
}

Common::Error TrecisionEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	byte version = NlVer;
	// TODO: Check for newer save versions
	Common::Serializer ser(nullptr, stream);
	ser.setVersion(version);
	stream->writeByte(version);
	syncGameStream(ser);
	return Common::kNoError;
}

bool TrecisionEngine::syncGameStream(Common::Serializer &ser) {
	// TODO: Get description from metadata
	char desc[40] = "savegame";
	ser.syncBytes((byte *)desc, 40);

	uint16 *thumbnailBuf = Icone + (READICON + 13) * ICONDX * ICONDY;
	ser.syncBytes((byte *)thumbnailBuf, ICONDX * ICONDY * sizeof(uint16));
	if (ser.isLoading())
		_graphicsMgr->updatePixelFormat(thumbnailBuf, ICONDX * ICONDY);

	ser.syncAsUint16LE(_curRoom);
	ser.syncAsByte(/*OldInvLen*/ _inventorySize);
	ser.syncAsByte(_cyberInventorySize);
	ser.syncAsByte(/*OldIconBase*/ _iconBase);
	ser.syncAsSint16LE(Flagskiptalk);
	ser.syncAsSint16LE(Flagskipenable);
	ser.syncAsSint16LE(_flagMouseEnabled);
	ser.syncAsSint16LE(_flagScreenRefreshed);
	ser.syncAsSint16LE(FlagPaintCharacter);
	ser.syncAsSint16LE(FlagSomeOneSpeak);
	ser.syncAsSint16LE(FlagCharacterSpeak);
	ser.syncAsSint16LE(_flagInventoryLocked);
	ser.syncAsSint16LE(FlagUseWithStarted);
	ser.syncAsSint16LE(FlagMousePolling);
	ser.syncAsSint16LE(FlagDialogSolitaire);
	ser.syncAsSint16LE(FlagCharacterExist);
	ser.syncBytes(/*OldInv*/ _inventory, MAXICON);
	ser.syncBytes(_cyberInventory, MAXICON);
	ser.syncAsFloatLE(_actor->_px);
	ser.syncAsFloatLE(_actor->_py);
	ser.syncAsFloatLE(_actor->_pz);
	ser.syncAsFloatLE(_actor->_dx);
	ser.syncAsFloatLE(_actor->_dz);
	ser.syncAsFloatLE(_actor->_theta);
	ser.syncAsSint32LE(_curPanel);
	ser.syncAsSint32LE(_oldPanel);

	for (int a = 0; a < MAXROOMS; a++) {
		ser.syncBytes((byte *)_room[a]._baseName, 4);
		for (int i = 0; i < MAXACTIONINROOM; i++)
			ser.syncAsUint16LE(_room[a]._actions[i]);
		ser.syncAsByte(_room[a]._flag);
		ser.syncAsUint16LE(_room[a]._bkgAnim);
	}

	for (int a = 0; a < MAXOBJ; a++) {
		for (int i = 0; i < 4; i++)
			ser.syncAsUint16LE(_obj[a]._lim[i]);
		ser.syncAsUint16LE(_obj[a]._name);
		ser.syncAsUint16LE(_obj[a]._examine);
		ser.syncAsUint16LE(_obj[a]._action);
		ser.syncAsUint16LE(_obj[a]._anim);
		ser.syncAsByte(_obj[a]._mode);
		ser.syncAsByte(_obj[a]._flag);
		ser.syncAsByte(_obj[a]._goRoom);
		ser.syncAsByte(_obj[a]._nbox);
		ser.syncAsByte(_obj[a]._ninv);
		ser.syncAsSByte(_obj[a]._position);
	}

	for (int a = 0; a < MAXINVENTORY; a++) {
		ser.syncAsUint16LE(_inventoryObj[a]._name);
		ser.syncAsUint16LE(_inventoryObj[a]._examine);
		ser.syncAsUint16LE(_inventoryObj[a]._action);
		ser.syncAsUint16LE(_inventoryObj[a]._anim);
		ser.syncAsByte(_inventoryObj[a]._flag);
	}

	for (int a = 0; a < MAXANIM; a++) {
		SAnim *cur = &_animMgr->_animTab[a];
		ser.syncBytes((byte *)cur->_name, 14);
		ser.syncAsUint16LE(cur->_flag);
		for (int i = 0; i < MAXCHILD; i++) {
			for (int j = 0; j < 4; j++) {
				ser.syncAsUint16LE(cur->_lim[i][j]);
			}
		}
		ser.syncAsByte(cur->_nbox);
		for (int i = 0; i < MAXATFRAME; i++) {
			ser.syncAsByte(cur->_atFrame[i]._type);
			ser.syncAsByte(cur->_atFrame[i]._child);
			ser.syncAsUint16LE(cur->_atFrame[i]._numFrame);
			ser.syncAsUint16LE(cur->_atFrame[i]._index);
		}
	}

	for (int a = 0; a < MAXSAMPLE; a++) {
		ser.syncAsByte(GSample[a]._volume);
		ser.syncAsByte(GSample[a]._flag);
	}

	for (int a = 0; a < MAXCHOICE; a++) {
		DialogChoice *cur = &_choice[a];
		ser.syncAsUint16LE(cur->_flag);
		ser.syncAsUint16LE(cur->_sentenceIndex);
		ser.syncAsUint16LE(cur->_firstSubTitle);
		ser.syncAsUint16LE(cur->_subTitleNumb);
		for (int i = 0; i < MAXDISPSCELTE; i++)
			ser.syncAsUint16LE(cur->_on[i]);
		for (int i = 0; i < MAXDISPSCELTE; i++)
			ser.syncAsUint16LE(cur->_off[i]);
		ser.syncAsUint16LE(cur->_startFrame);
		ser.syncAsUint16LE(cur->_nextDialog);
	}

	for (int a = 0; a < MAXDIALOG; a++) {
		Dialog *cur = &_dialog[a];
		ser.syncAsUint16LE(cur->_flag);
		ser.syncAsUint16LE(cur->_interlocutor);
		ser.syncBytes((byte *)cur->_startAnim, 14);
		ser.syncAsUint16LE(cur->_startLen);
		ser.syncAsUint16LE(cur->_firstChoice);
		ser.syncAsUint16LE(cur->_choiceNumb);
		for (int i = 0; i < MAXNEWSMKPAL; i++)
			ser.syncAsUint16LE(cur->_newPal[i]);
	}

	for (int i = 0; i < 7; i++)
		ser.syncAsUint16LE(_logicMgr->Comb35[i]);
	for (int i = 0; i < 4; i++)
		ser.syncAsUint16LE(_logicMgr->Comb49[i]);
	for (int i = 0; i < 6; i++)
		ser.syncAsUint16LE(_logicMgr->Comb4CT[i]);
	for (int i = 0; i < 6; i++)
		ser.syncAsUint16LE(_logicMgr->Comb58[i]);
	for (int i = 0; i < 3; i++)
		ser.syncAsUint16LE(_wheelPos[i]);
	ser.syncAsUint16LE(_wheel);
	ser.syncAsUint16LE(_logicMgr->Count35);
	ser.syncAsUint16LE(_logicMgr->Count58);
	ser.syncAsUint16LE(_slotMachine41Counter);

	return true;
}

/*-----------------03/01/97 16.15-------------------
					InitMain
--------------------------------------------------*/
void TrecisionEngine::initMain() {
	for (int c = 0; c < MAXOBJ; c++)
		_obj[c]._position = -1;

	initNames();
	_logicMgr->initScript();
	openSys();

	LoadAll();

	initMessageSystem();
	_logicMgr->initInventory();

	_curRoom = rINTRO;

	ProcessTime();

	doEvent(MC_SYSTEM, ME_START, MP_DEFAULT, 0, 0, 0, 0);
}

/*-------------------------------------------------------------------------*/
/*                            initMessageSystem          				   */
/*-------------------------------------------------------------------------*/
void TrecisionEngine::initMessageSystem() {
	_gameQueue.initQueue();
	_animQueue.initQueue();
	_characterQueue.initQueue();
	for (uint8 i = 0; i < MAXMESSAGE; i++) {
		_gameQueue._event[i] = &_gameMsg[i];
		_characterQueue._event[i] = &_characterMsg[i];
		_animQueue._event[i] = &_animMsg[i];
	}
}

/* --------------------------------------------------
 * 						LoadAll
 * --------------------------------------------------*/
void TrecisionEngine::LoadAll() {
	Common::File dataNl;
	if (!dataNl.open("DATA.NL"))
		error("LoadAll : Couldn't open DATA.NL");

	for (int i = 0; i < MAXROOMS; ++i) {
		dataNl.read(&_room[i]._baseName, ARRAYSIZE(_room[i]._baseName));
		_room[i]._flag = dataNl.readByte();
		dataNl.readByte(); // Padding
		_room[i]._bkgAnim = dataNl.readUint16LE();
		for (int j = 0; j < MAXOBJINROOM; ++j)
			_room[i]._object[j] = dataNl.readUint16LE();
		for (int j = 0; j < MAXSOUNDSINROOM; ++j)
			_room[i]._sounds[j] = dataNl.readUint16LE();
		for (int j = 0; j < MAXACTIONINROOM; ++j)
			_room[i]._actions[j] = dataNl.readUint16LE();
	}

	for (int i = 0; i < MAXOBJ; ++i) {
		_obj[i]._dx = dataNl.readUint16LE();
		_obj[i]._dy = dataNl.readUint16LE();
		_obj[i]._px = dataNl.readUint16LE();
		_obj[i]._py = dataNl.readUint16LE();

		for (int j = 0; j < 4; ++j)
			_obj[i]._lim[j] = dataNl.readUint16LE();

		_obj[i]._position = dataNl.readSByte();
		dataNl.readByte(); // Padding
		_obj[i]._name = dataNl.readUint16LE();
		_obj[i]._examine = dataNl.readUint16LE();
		_obj[i]._action = dataNl.readUint16LE();
		_obj[i]._goRoom = dataNl.readByte();
		_obj[i]._nbox = dataNl.readByte();
		_obj[i]._ninv = dataNl.readByte();
		_obj[i]._mode = dataNl.readByte();
		_obj[i]._flag = dataNl.readByte();
		dataNl.readByte(); // Padding
		_obj[i]._anim = dataNl.readUint16LE();
	}

	for (int i = 0; i < MAXINVENTORY; ++i) {
		_inventoryObj[i]._name = dataNl.readUint16LE();
		_inventoryObj[i]._examine = dataNl.readUint16LE();
		_inventoryObj[i]._action = dataNl.readUint16LE();
		_inventoryObj[i]._flag = dataNl.readByte();
		dataNl.readByte(); // Padding
		_inventoryObj[i]._anim = dataNl.readUint16LE();
	}

	for (int i = 0; i < MAXSAMPLE; ++i) {
		dataNl.read(&GSample[i]._name, ARRAYSIZE(GSample[i]._name));
		GSample[i]._volume = dataNl.readByte();
		GSample[i]._flag = dataNl.readByte();
		GSample[i]._panning = dataNl.readSByte();
	}

	for (int i = 0; i < MAXSCRIPTFRAME; ++i) {
		_scriptFrame[i]._class = dataNl.readByte();
		_scriptFrame[i]._event = dataNl.readByte();
		_scriptFrame[i]._u8Param = dataNl.readByte();
		dataNl.readByte(); // Padding
		_scriptFrame[i]._u16Param1 = dataNl.readUint16LE();
		_scriptFrame[i]._u16Param2 = dataNl.readUint16LE();
		_scriptFrame[i]._u32Param = dataNl.readUint16LE();
		_scriptFrame[i]._noWait = !(dataNl.readSint16LE() == 0);
	}

	for (int i = 0; i < MAXSCRIPT; ++i) {
		_script[i]._firstFrame = dataNl.readUint16LE();
		_script[i]._flag = dataNl.readByte();
		dataNl.readByte(); // Padding
	}

	for (int i = 0; i < MAXANIM; ++i) {
		dataNl.read(&_animMgr->_animTab[i]._name, ARRAYSIZE(_animMgr->_animTab[i]._name));

		_animMgr->_animTab[i]._flag = dataNl.readUint16LE();

		for (int j = 0; j < MAXCHILD; ++j) {
			_animMgr->_animTab[i]._lim[j][0] = dataNl.readUint16LE();
			_animMgr->_animTab[i]._lim[j][1] = dataNl.readUint16LE();
			_animMgr->_animTab[i]._lim[j][2] = dataNl.readUint16LE();
			_animMgr->_animTab[i]._lim[j][3] = dataNl.readUint16LE();
		}

		_animMgr->_animTab[i]._nbox = dataNl.readByte();
		dataNl.readByte(); // Padding

		for (int j = 0; j < MAXATFRAME; ++j) {
			_animMgr->_animTab[i]._atFrame[j]._type = dataNl.readByte();
			_animMgr->_animTab[i]._atFrame[j]._child = dataNl.readByte();
			_animMgr->_animTab[i]._atFrame[j]._numFrame = dataNl.readUint16LE();
			_animMgr->_animTab[i]._atFrame[j]._index = dataNl.readUint16LE();
		}
	}

	for (int i = 0; i < MAXDIALOG; ++i) {
		_dialog[i]._flag = dataNl.readUint16LE();
		_dialog[i]._interlocutor = dataNl.readUint16LE();

		dataNl.read(&_dialog[i]._startAnim, ARRAYSIZE(_dialog[i]._startAnim));

		_dialog[i]._startLen = dataNl.readUint16LE();
		_dialog[i]._firstChoice = dataNl.readUint16LE();
		_dialog[i]._choiceNumb = dataNl.readUint16LE();

		for (int j = 0; j < MAXNEWSMKPAL; ++j)
			_dialog[i]._newPal[j] = dataNl.readUint16LE();
	}

	for (int i = 0; i < MAXCHOICE; ++i) {
		_choice[i]._flag = dataNl.readUint16LE();
		_choice[i]._sentenceIndex = dataNl.readUint16LE();
		_choice[i]._firstSubTitle = dataNl.readUint16LE();
		_choice[i]._subTitleNumb = dataNl.readUint16LE();

		for (int j = 0; j < MAXDISPSCELTE; ++j)
			_choice[i]._on[j] = dataNl.readUint16LE();

		for (int j = 0; j < MAXDISPSCELTE; ++j)
			_choice[i]._off[j] = dataNl.readUint16LE();

		_choice[i]._startFrame = dataNl.readUint16LE();
		_choice[i]._nextDialog = dataNl.readUint16LE();
	}

	for (int i = 0; i < MAXSUBTITLES; ++i) {
		_subTitles[i]._sentence = dataNl.readUint16LE();
		_subTitles[i]._x = dataNl.readUint16LE();
		_subTitles[i]._y = dataNl.readUint16LE();
		_subTitles[i]._color = dataNl.readUint16LE();
		_subTitles[i]._startFrame = dataNl.readUint16LE();
		_subTitles[i]._length = dataNl.readUint16LE();
	}

	for (int i = 0; i < MAXACTION; ++i)
		_actionLen[i] = dataNl.readByte();

	NumFileRef = dataNl.readSint32LE();

	for (int i = 0; i < NumFileRef; ++i) {
		dataNl.read(&FileRef[i].name, ARRAYSIZE(FileRef[i].name));
		FileRef[i].offset = dataNl.readSint32LE();
	}

	dataNl.read(TextArea, MAXTEXTAREA);

	_textPtr = (char *)TextArea;

	for (int a = 0; a < MAXOBJNAME; a++)
		_objName[a] = getNextSentence();

	for (int a = 0; a < MAXSENTENCE; a++)
		_sentence[a] = getNextSentence();

	for (int a = 0; a < MAXSYSTEXT; a++)
		_sysText[a] = getNextSentence();

	dataNl.close();
}

/*-------------------------------------------------
					checkSystem
 --------------------------------------------------*/
void TrecisionEngine::checkSystem() {
	_animMgr->refreshAllAnimations();
	eventLoop();
}

void TrecisionEngine::initCursor() {
	const int cw = 21, ch = 21;
	const int cx = 10, cy = 10;
	uint16 cursor[cw * ch];
	memset(cursor, 0, ARRAYSIZE(cursor) * 2);

	const uint16 cursorColor = _graphicsMgr->palTo16bit(255, 255, 255);

	for (int i = 0; i < cw; i++) {
		if (i >= 8 && i <= 12 && i != 10)
			continue;
		cursor[cx * cw + i] = cursorColor;	// horizontal
		cursor[cx + cw * i] = cursorColor;	// vertical
	}

	Graphics::PixelFormat format = g_system->getScreenFormat();
	CursorMan.pushCursor(cursor, cw, ch, cx, cy, 0, false, &format);
}

SActor::SActor(TrecisionEngine *vm) : _vm(vm) {
	_vertex = nullptr;
	_face = nullptr;
	_light = nullptr;
	_camera = nullptr;
	_texture = nullptr;

	_vertexNum = 0;
	_faceNum = 0;
	_lightNum = 0;
	_matNum = 0;

	_px = _py = _pz = 0.0;
	_dx = _dz = 0.0;
	_theta = 0.0;

	for (int i = 0; i < 6; ++i)
		_lim[i] = 0;

	_curFrame = 0;
	_curAction = 0;

	for (int i = 0; i < 256; ++i) {
		for (int j = 0; j < 91; ++j)
			_textureMat[i][j] = 0;
	}

	for (int i = 0; i < MAXFACE; ++i) {
		for (int j = 0; j < 3; ++j) {
			_textureCoord[i][j][0] = 0;
			_textureCoord[i][j][1] = 0;
		}
	}

	_characterArea = nullptr;
}

SActor::~SActor() {
	delete[] _characterArea;
	delete[] _face;
//	delete _light;
//	delete _camera;
//	delete _texture;
}

} // End of namespace Trecision