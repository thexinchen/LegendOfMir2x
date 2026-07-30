#include "mirevent.hpp"
#include <algorithm>
#include "log.hpp"
#include "pathf.hpp"
#include "totype.hpp"
#include "dbcomid.hpp"
#include "clientmonster.hpp"
#include "uidf.hpp"
#include "mathf.hpp"
#include "fflerror.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "processrun.hpp"
#include "protocoldef.hpp"
#include "gui_texture.hpp"
#include "clientargparser.hpp"
#include "clientpathfinder.hpp"
#include "creaturemovable.hpp"
#include "motioneffect.hpp"

extern Log *g_mir2xLog;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;
extern FontexDB *g_fontexDB;
extern PNGTexOffDB *g_monsterDB;
extern ClientArgParser *g_clientArgParser;

std::optional<uint32_t> MonsterFrameGfxSeq::gfxID(const ClientMonster *monPtr, std::optional<int> frameOpt) const
{
    fflassert(monPtr);
    if(*this){
        // monster graphics retrieving key structure
        //
        //   3322 2222 2222 1111 1111 1100 0000 0000
        //   1098 7654 3210 9876 5432 1098 7654 3210
        //   ^^^^ ^^^^ ^^^^ ^^^^ ^^^^ ^^^^ ^^^^ ^^^^
        //   |||| |||| |||| |||| |||| |||| |||| ||||
        //             |||| |||| |||| |||| |||+-++++-----------     frame : max =   32
        //             |||| |||| |||| |||| +++----------------- direction : max =    8 -+
        //             |||| |||| |||| ++++---------------------    motion : max =   16 -+
        //             |+++-++++-++++--------------------------      look : max = 2048 -+------> gfxBaseID
        //             +---------------------------------------    shadow : max =    2
        //

        const auto frame = frameOpt.value_or(monPtr->currMotion()->frame);
        fflassert(frame >= 0);
        fflassert(frame < count);

        const auto          gfxFrame = to_u32(begin + frame * (reverse ? -1 : 1));
        const auto      lookGfxIndex = to_u32(gfxLookID.value_or(monPtr->getMR().lookID) - LID_BEGIN);
        const auto    motionGfxIndex = to_u32(gfxMotionID.value_or(monPtr->currMotion()->type) - MOTION_MON_BEGIN);
        const auto directionGfxIndex = to_u32(gfxDirectionID.value_or(monPtr->currMotion()->direction) - DIR_BEGIN);

        return 0
            + ((     lookGfxIndex & 0X07FF) << 12)
            + ((   motionGfxIndex & 0X000F) <<  8)
            + ((directionGfxIndex & 0X0007) <<  5)
            + ((         gfxFrame & 0X001F) <<  0);
    }
    return {};
}

ClientMonster::ClientMonster(uint64_t uid, ProcessRun *proc)
    : CreatureMovable(uid, proc)
{
    fflassert(uidf::getUIDType(uid) == UID_MON, uidf::getUIDString(uid));
    if(g_clientArgParser->drawUID){
        m_nameBoard = str_printf(u8"%s(%llu)", DBCOM_MONSTERRECORD(monsterID()).name, to_llu(UID()));
    }
    else{
        m_nameBoard = str_printf(u8"%s", DBCOM_MONSTERRECORD(monsterID()).name);
    }
}

bool ClientMonster::update(double ms)
{
    updateAttachMagic(ms);
    const CallOnExitHelper motionOnUpdate([lastSeqFrameID = m_currMotion->getSeqFrameID(), this]()
    {
        m_currMotion->runTrigger();
        if(lastSeqFrameID == m_currMotion->getSeqFrameID()){
            return;
        }

        switch(m_currMotion->type){
            case MOTION_MON_SPAWN:
                {
                    if(m_currMotion->frame == 0){
                        playSoundEffect(getSeffID(MONSEFF_SPAWN));
                    }
                    break;
                }
            case MOTION_MON_STAND: // shall only play when show up
                {
                    break;
                }
            case MOTION_MON_ATTACK0:
            case MOTION_MON_ATTACK1:
                {
                    if(m_currMotion->frame == 0){
                        playSoundEffect(getSeffID(MONSEFF_ATTACK));
                    }
                    break;
                }
            case MOTION_MON_HITTED:
                {
                    if(m_currMotion->frame == 0){
                        playSoundEffect(getSeffID(MONSEFF_HITTED));
                        switch(const auto fromUID = m_currMotion->extParam.hitted.fromUID; uidf::getUIDType(fromUID)){
                            case UID_MON:
                                {
                                    playSoundEffect(0X01010000 + 61);
                                    break;
                                }
                            case UID_PLY:
                                {
                                    playSoundEffect([fromUID, this]() -> uint32_t
                                    {
                                        if(const auto plyPtr = m_processRun->findUID(fromUID)){
                                            if(const auto itemID = dynamic_cast<const Hero *>(plyPtr)->getWLItem(WLG_WEAPON).itemID){
                                                const auto &ir = DBCOM_ITEMRECORD(itemID);
                                                fflassert(ir);

                                                if     (ir.equip.weapon.category == u8"匕首") return 0X01010000 + 60;
                                                else if(ir.equip.weapon.category == u8"木剑") return 0X01010000 + 61;
                                                else if(ir.equip.weapon.category == u8"剑"  ) return 0X01010000 + 62;
                                                else if(ir.equip.weapon.category == u8"刀"  ) return 0X01010000 + 63;
                                                else if(ir.equip.weapon.category == u8"斧"  ) return 0X01010000 + 64;
                                                else if(ir.equip.weapon.category == u8"锏"  ) return 0X01010000 + 65;
                                            }
                                        }
                                        return 0X01010000 + 61; // use 木剑 sound effect as default
                                    }());
                                    break;
                                }
                            default:
                                {
                                    break;
                                }
                        }
                    }
                    break;
                }
            case MOTION_MON_DIE:
                {
                    if(m_currMotion->frame == 0){
                        playSoundEffect(getSeffID(MONSEFF_DIE));
                    }
                    break;
                }
            default:
                {
                    break;
                }
        }
    });

    if(m_currMotion->effect && !m_currMotion->effect->done()){
        m_currMotion->effect->update(ms);
        return true;
    }

    if(!checkUpdate(ms)){
        return true;
    }

    switch(m_currMotion->type){
        case MOTION_MON_STAND:
            {
                if(stayIdle()){
                    return advanceMotionFrame();
                }

                // move to next motion will reset frame as 0
                // if current there is no more motion pending
                // it will add a MOTION_MON_STAND
                //
                // we don't want to reset the frame here
                return moveNextMotion();
            }
        case MOTION_MON_DIE:
            {
                const auto frameCount = getFrameCount(m_currMotion.get());
                if(frameCount <= 0){
                    return false;
                }

                if(m_currMotion->frame + 1 < frameCount){
                    return advanceMotionFrame();
                }

                switch(m_currMotion->extParam.die.fadeOut){
                    case 0:
                        {
                            break;
                        }
                    case 255:
                        {
                            // deactivated if fadeOut reach 255
                            // next update will auotmatically delete it
                            break;
                        }
                    default:
                        {
                            int nextFadeOut = 0;
                            nextFadeOut = std::max<int>(1, m_currMotion->extParam.die.fadeOut + 10);
                            nextFadeOut = std::min<int>(nextFadeOut, 255);

                            m_currMotion->extParam.die.fadeOut = nextFadeOut;
                            break;
                        }
                }
                return true;
            }
        default:
            {
                return updateMotion(true);
            }
    }
}

void ClientMonster::drawFrame(int viewX, int viewY, int focusMask, int frame, bool frameOnly)
{
    const auto gfxBodyIDOpt = getFrameGfxSeq(m_currMotion->type, m_currMotion->direction).gfxID(this, frame);
    if(!gfxBodyIDOpt.has_value()){
        return;
    }

    const uint32_t   bodyKey = (to_u32(0) << 23) + gfxBodyIDOpt.value(); // body
    const uint32_t shadowKey = (to_u32(1) << 23) + gfxBodyIDOpt.value(); // shadow

    const auto [  bodyFrame,   bodyDX,   bodyDY] = g_monsterDB->retrieve(  bodyKey);
    const auto [shadowFrame, shadowDX, shadowDY] = g_monsterDB->retrieve(shadowKey);
    const auto [shiftX, shiftY] = getShift(frame);

    // fadeOut :     0: normal
    //         : 1-255: fading out (body and shadow both decay)
    const auto [bodyAlpha, shadowAlpha] = [this]() -> std::tuple<uint8_t, uint8_t>
    {
        if((m_currMotion->type == MOTION_MON_DIE) && (m_currMotion->extParam.die.fadeOut > 0)){
            const auto fadeOut = m_currMotion->extParam.die.fadeOut;
            const auto alpha   = to_u8(255 - fadeOut);

            return {alpha, alpha / 2};
        }
        return {255, 128};
    }();

    const auto fnBlendFrame = [](GLTexID pTexture, int nFocusChan, uint8_t alpha, int nX, int nY)
    {
        if(true
                && pTexture
                && nFocusChan >= 0
                && nFocusChan <  FOCUS_END){

            // if provided channel as 0
            // just blend it using the original color

            const GLDeviceHelper::EnableTextureModColor modColor(pTexture, colorf::MirColor2RGBA(focusColor(nFocusChan, alpha)));
            g_glDevice->drawTexture(pTexture, nX, nY);
        }
    };

    const int startX = currMotion()->x * SYS_MAPGRIDXP + shiftX - viewX;
    const int startY = currMotion()->y * SYS_MAPGRIDYP + shiftY - viewY;

    if(getMR().shadow){
        fnBlendFrame(shadowFrame, 0, shadowAlpha, startX + shadowDX, startY + shadowDY);
    }
    fnBlendFrame(bodyFrame, 0, bodyAlpha, startX + bodyDX, startY + bodyDY);

    if(!frameOnly){
        if(g_clientArgParser->drawTextureAlignLine){
            g_glDevice->drawLine (colorf::RED  + colorf::A_SHF(128), startX, startY, startX + bodyDX, startY + bodyDY);
            g_glDevice->drawCross(colorf::BLUE + colorf::A_SHF(128), startX, startY, 5);

            const auto [texW, texH] = GLDeviceHelper::getTextureSize(bodyFrame);
            g_glDevice->drawRectangle(colorf::RED + colorf::A_SHF(128), startX + bodyDX, startY + bodyDY, texW, texH);
        }

        if(g_clientArgParser->drawTargetBox){
            if(const auto box = getTargetBox()){
                g_glDevice->drawRectangle(colorf::BLUE + colorf::A_SHF(128), box.x - viewX, box.y - viewY, box.w, box.h);
            }
        }
    }

    for(int nFocusChan = 1; nFocusChan < FOCUS_END; ++nFocusChan){
        if(focusMask & (1 << nFocusChan)){
            fnBlendFrame(bodyFrame, nFocusChan, bodyAlpha, startX + bodyDX, startY + bodyDY);
        }
    }

    if(!frameOnly){
        for(auto &p: m_attachMagicList){
            p->drawShift(startX, startY, colorf::RGBA(0XFF, 0XFF, 0XFF, 0XFF));
        }

        if(m_currMotion->effect && !m_currMotion->effect->done()){
            m_currMotion->effect->drawShift(startX, startY, colorf::RGBA(0XFF, 0XFF, 0XFF, 0XF0));
        }

        if(m_currMotion->type != MOTION_MON_DIE && g_clientArgParser->drawHPBar){
            auto pBar0 = g_progUseDB->retrieve(0X00000014);
            auto pBar1 = g_progUseDB->retrieve(0X00000015);

            const auto [nBarW, nBarH] = GLDeviceHelper::getTextureSize(pBar1);
            const int drawBarXP = startX +  7;
            const int drawBarYP = startY - 53;
            const int drawBarWidth = to_d(std::lround(nBarW * getHealthRatio().at(0)));

            g_glDevice->drawTexture(pBar1, drawBarXP, drawBarYP, 0, 0, drawBarWidth, nBarH);
            g_glDevice->drawTexture(pBar0, drawBarXP, drawBarYP);

            constexpr int buffIconDrawW = 10;
            constexpr int buffIconDrawH = 10;

            const int buffIconStartX = drawBarXP + 1;
            const int buffIconStartY = drawBarYP - buffIconDrawH;

            if(getSDBuffIDListOpt().has_value()){
                for(int drawIconCount = 0; const auto id: getSDBuffIDListOpt().value().idList){
                    const auto &br = DBCOM_BUFFRECORD(id);
                    fflassert(br);

                    if(br.icon.gfxID != SYS_U32NIL){
                        if(auto iconTexPtr = g_progUseDB->retrieve(br.icon.gfxID)){
                            const int buffIconOffX = buffIconStartX + (drawIconCount % 3) * buffIconDrawW;
                            const int buffIconOffY = buffIconStartY - (drawIconCount / 3) * buffIconDrawH;

                            const auto [texW, texH] = GLDeviceHelper::getTextureSize(iconTexPtr);
                            g_glDevice->drawTexture(iconTexPtr, buffIconOffX, buffIconOffY, buffIconDrawW, buffIconDrawH, 0, 0, texW, texH);

                            const auto baseColor = [&br]() -> uint32_t
                            {
                                if(br.favor > 0){
                                    return colorf::GREEN;
                                }
                                else if(br.favor == 0){
                                    return colorf::YELLOW;
                                }
                                else{
                                    return colorf::RED;
                                }
                            }();

                            const auto startColor = baseColor | colorf::A_SHF(255);
                            const auto   endColor = baseColor | colorf::A_SHF( 64);

                            const auto edgeGridCount = (buffIconDrawW + buffIconDrawH) * 2 - 4;
                            const auto startLoc = std::lround(edgeGridCount * std::fmod(m_accuUpdateTime, 1500.0) / 1500.0);

                            g_glDevice->drawBoxFading(startColor, endColor, buffIconOffX, buffIconOffY, buffIconDrawW, buffIconDrawH, startLoc, buffIconDrawW + buffIconDrawH);
                            drawIconCount++;
                        }
                    }
                }
            }

            if(g_clientArgParser->alwaysDrawName || (focusMask & (1 << FOCUS_MOUSE))){
                if(const auto nameTexture = g_fontexDB->retrieve(11, 15, 0, to_cstr(m_nameBoard.c_str())); nameTexture){
                    const int nDrawNameXP = drawBarXP + nBarW / 2 - nameTexture.w / 2;
                    const int nDrawNameYP = drawBarYP + 20;
                    g_glDevice->drawTexture(nameTexture, nDrawNameXP, nDrawNameYP);
                }
            }
        }
    }
}

static uint32_t monsterSeffBaseIDHelper(std::u8string_view monName, int offset)
{
    fflassert(!monName.empty());
    fflassert(offset >= 0, offset);
    fflassert(offset < to_d(SYS_SEFFSIZE), offset, SYS_SEFFSIZE);

    const auto &mr = DBCOM_MONSTERRECORD(monName.data());
    fflassert(mr);

    if(mr.seff.ref.empty()){
        if(mr.seff.list.at(offset).has_value()){
            if(const auto &[subname, suboff] = mr.seff.list.at(offset).value(); subname.empty()){
                return suboff;
            }
            else{
                return monsterSeffBaseIDHelper(subname, offset);
            }
        }
        else{
            return SYS_MONSEFFBASE(mr.lookID) + offset;
        }
    }
    else{
        return monsterSeffBaseIDHelper(mr.seff.ref, offset);
    }
}

uint32_t ClientMonster::getSeffID(int offset) const
{
    return monsterSeffBaseIDHelper(getMR().name, offset);
}

bool ClientMonster::parseAction(const ActionNode &action)
{
    m_lastActive = mirGetTicks();
    for(const auto &m: m_forcedMotionQueue){
        if(m->type == MOTION_MON_DIE){
            return true;
        }
    }

    for(const auto &m: m_motionQueue){
        if(m->type == MOTION_MON_DIE){
            throw fflpanic("Found MOTION_MON_DIE in pending motion queue");
        }
    }

    m_motionQueue.clear();
    switch(action.type){
        case ACTION_DIE      : return onActionDie      (action) && motionQueueValid();
        case ACTION_STAND    : return onActionStand    (action) && motionQueueValid();
        case ACTION_HITTED   : return onActionHitted   (action) && motionQueueValid();
        case ACTION_JUMP     : return onActionJump     (action) && motionQueueValid();
        case ACTION_MOVE     : return onActionMove     (action) && motionQueueValid();
        case ACTION_ATTACK   : return onActionAttack   (action) && motionQueueValid();
        case ACTION_SPAWN    : return onActionSpawn    (action) && motionQueueValid();
        case ACTION_TRANSF   : return onActionTransf   (action) && motionQueueValid();
        case ACTION_SPACEMOVE: return onActionSpaceMove(action) && motionQueueValid();
        default              : return false;
    }
}

bool ClientMonster::onActionDie(const ActionNode &action)
{
    const auto [endX, endY, endDir] = motionEndGLoc().at(1);
    for(auto &node: makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED)){
        if(!(node && motionValid(node))){
            throw fflpanic("current motion node is invalid");
        }
        m_forcedMotionQueue.push_back(std::move(node));
    }

    const auto [dieX, dieY, dieDir] = motionEndGLoc().at(1);
    m_forcedMotionQueue.emplace_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_DIE,
        .direction = pathf::dirValid(dieDir) ? to_d(dieDir) : DIR_UP,
        .x = dieX,
        .y = dieY,
    }));

    if(const auto deathEffectName = str_printf(u8"%s_死亡特效", to_cstr(monsterName())); DBCOM_MAGICID(to_u8cstr(deathEffectName))){
        m_forcedMotionQueue.back()->addTrigger(true, [deathEffectName, this](MotionNode *) -> bool
        {
            addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(to_u8cstr(deathEffectName), u8"运行")));
            return true;
        });
    }

    // set motion fadeOut as 0
    // server later will issue fadeOut on dead body
    return true;
}

bool ClientMonster::onActionStand(const ActionNode &action)
{
    const auto [endX, endY, endDir] = motionEndGLoc().at(1);
    m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = action.direction,
        .x = action.x,
        .y = action.y,
    }));
    return true;
}

bool ClientMonster::onActionHitted(const ActionNode &action)
{
    const auto [endX, endY, endDir] = motionEndGLoc().at(1);
    m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
    m_motionQueue.emplace_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_HITTED,
        .direction = action.direction,
        .x = action.x,
        .y = action.y,
        .extParam
        {
            .hitted
            {
                .fromUID = action.fromUID,
            },
        },
    }));
    return true;
}

bool ClientMonster::onActionTransf(const ActionNode &)
{
    throw fflpanic("unexpected ACTION_TRANSF to uid: {}", uidf::getUIDString(UID()));
}

bool ClientMonster::onActionSpaceMove(const ActionNode &action)
{
    flushForcedMotion();
    m_currMotion.reset(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = m_currMotion->direction,
        .x = action.aimX,
        .y = action.aimY,
    });

    m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FixedLocMagic(u8"瞬息移动", u8"运行", action.x, action.y)));
    addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(u8"瞬息移动", u8"裂解")));
    return true;
}

bool ClientMonster::onActionJump(const ActionNode &action)
{
    flushForcedMotion();
    m_currMotion.reset(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = action.direction,
        .x = action.x,
        .y = action.y,
    });
    return true;
}

bool ClientMonster::onActionMove(const ActionNode &action)
{
    const auto [endX, endY, endDir] = motionEndGLoc().at(1);
    m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
    if(auto moveNode = makeWalkMotion(action.x, action.y, action.aimX, action.aimY, action.speed); motionValid(moveNode)){
        m_motionQueue.push_back(std::move(moveNode));
        return true;
    }
    return false;
}

bool ClientMonster::onActionSpawn(const ActionNode &action)
{
    if(!m_forcedMotionQueue.empty()){
        throw fflpanic("found motion before spawn: {}", uidf::getUIDString(UID()));
    }

    m_currMotion = std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = [&action]() -> int
        {
            if(pathf::dirValid(action.direction)){
                return action.direction;
            }
            return DIR_UP;
        }(),

        .x = action.x,
        .y = action.y,
    });
    return true;
}

bool ClientMonster::onActionAttack(const ActionNode &action)
{
    const auto [endX, endY, endDir] = motionEndGLoc().at(1);
    m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
    if(auto coPtr = m_processRun->findUID(action.aimUID)){
        m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
        {
            .type = MOTION_MON_ATTACK0,
            .direction = [&action, endDir, coPtr]() -> int
            {
                const auto nX = coPtr->x();
                const auto nY = coPtr->y();
                if(mathf::LDistance2<int>(nX, nY, action.x, action.y) == 0){
                    return endDir;
                }
                return pathf::getOffDir(action.x, action.y, nX, nY);
            }(),
            .x = action.x,
            .y = action.y,
        }));
        return true;
    }
    return false;
}

bool ClientMonster::motionValid(const std::unique_ptr<MotionNode> &motionPtr) const
{
    if(true
            && motionPtr
            && motionPtr->type >= MOTION_MON_BEGIN
            && motionPtr->type <  MOTION_MON_END

            && motionPtr->direction >= DIR_BEGIN
            && motionPtr->direction <  DIR_END

            && m_processRun
            && m_processRun->onMap(m_processRun->mapUID(), motionPtr->x,    motionPtr->y)
            && m_processRun->onMap(m_processRun->mapUID(), motionPtr->endX, motionPtr->endY)

            && motionPtr->speed >= SYS_MINSPEED
            && motionPtr->speed <= SYS_MAXSPEED

            && motionPtr->frame >= 0
            && motionPtr->frame <  getFrameCount(motionPtr.get())){

        const auto nLDistance2 = mathf::LDistance2(motionPtr->x, motionPtr->y, motionPtr->endX, motionPtr->endY);
        switch(motionPtr->type){
            case MOTION_MON_STAND:
                {
                    return nLDistance2 == 0;
                }
            case MOTION_MON_WALK:
                {
                    return false
                        || nLDistance2 == 1
                        || nLDistance2 == 2
                        || nLDistance2 == 1 * maxStep() * maxStep()
                        || nLDistance2 == 2 * maxStep() * maxStep();
                }
            case MOTION_MON_ATTACK0:
            case MOTION_MON_ATTACK1:
            case MOTION_MON_SPELL0:
            case MOTION_MON_SPELL1:
            case MOTION_MON_HITTED:
            case MOTION_MON_DIE:
            case MOTION_MON_SPAWN:
                {
                    return nLDistance2 == 0;
                }
            case MOTION_MON_SPECIAL:
                {
                    return true;
                }
            default:
                {
                    break;
                }
        }
    }
    return false;
}

MonsterFrameGfxSeq ClientMonster::getFrameGfxSeq(int motion, int) const
{
    switch(motion){
        case MOTION_MON_STAND  : return {.count =  4};
        case MOTION_MON_WALK   : return {.count =  6};
        case MOTION_MON_ATTACK0: return {.count =  6};
        case MOTION_MON_HITTED : return {.count =  2};
        case MOTION_MON_DIE    : return {.count = 10};
        case MOTION_MON_ATTACK1: return {.count =  6};
        case MOTION_MON_SPELL0 :
        case MOTION_MON_SPELL1 : return {.count = 10};
        case MOTION_MON_SPAWN  : return {.count = 10};
        case MOTION_MON_SPECIAL: return {.count =  6};
        default                : return {};
    }
}

std::unique_ptr<MotionNode> ClientMonster::makeWalkMotion(int nX0, int nY0, int nX1, int nY1, int nSpeed) const
{
    if(true
            && m_processRun
            && m_processRun->canMove(true, 0, nX0, nY0)
            && m_processRun->canMove(true, 0, nX1, nY1)

            && nSpeed >= SYS_MINSPEED
            && nSpeed <= SYS_MAXSPEED){

        static const int nDirV[][3] = {
            {DIR_UPLEFT,   DIR_UP,   DIR_UPRIGHT  },
            {DIR_LEFT,     DIR_NONE, DIR_RIGHT    },
            {DIR_DOWNLEFT, DIR_DOWN, DIR_DOWNRIGHT}};

        int nSDX = 1 + (nX1 > nX0) - (nX1 < nX0);
        int nSDY = 1 + (nY1 > nY0) - (nY1 < nY0);

        auto nLDistance2 = mathf::LDistance2(nX0, nY0, nX1, nY1);
        if(false
                || nLDistance2 == 1
                || nLDistance2 == 2
                || nLDistance2 == 1 * maxStep() * maxStep()
                || nLDistance2 == 2 * maxStep() * maxStep()){

            return std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_WALK,
                .direction = nDirV[nSDY][nSDX],
                .speed = nSpeed,
                .x = nX0,
                .y = nY0,
                .endX = nX1,
                .endY = nY1,
            });
        }
    }
    return {};
}

ClientCreature::TargetBox ClientMonster::getTargetBox() const
{
    switch(m_currMotion->type){
        case MOTION_MON_DIE:
            {
                return {};
            }
        default:
            {
                break;
            }
    }

    const auto texBodyID = getFrameGfxSeq(m_currMotion->type, m_currMotion->direction).gfxID(this);
    if(!texBodyID.has_value()){
        return {};
    }

    int dx = 0;
    int dy = 0;
    auto bodyFrameTexPtr = g_monsterDB->retrieve(texBodyID.value(), &dx, &dy);

    if(!bodyFrameTexPtr){
        return {};
    }

    const auto [bodyFrameW, bodyFrameH] = GLDeviceHelper::getTextureSize(bodyFrameTexPtr);

    const auto [shiftX, shiftY] = getShift(m_currMotion->frame);
    const int startX = m_currMotion->x * SYS_MAPGRIDXP + shiftX + dx;
    const int startY = m_currMotion->y * SYS_MAPGRIDYP + shiftY + dy;

    return getTargetBoxHelper(startX, startY, bodyFrameW, bodyFrameH);
}

bool ClientMonster::deadFadeOut()
{
    switch(m_currMotion->type){
        case MOTION_MON_DIE:
            {
                if(getMR().deadFadeOut){
                    if(!m_currMotion->extParam.die.fadeOut){
                        m_currMotion->extParam.die.fadeOut = 1;
                    }
                }
                return true;
            }
        default:
            {
                return false; // TODO push an ActionDie here
            }
    }
}

int ClientMonster::maxStep() const
{
    return 1;
}

int ClientMonster::currStep() const
{
    fflassert(motionValid(m_currMotion));
    switch(m_currMotion->type){
        case MOTION_MON_WALK:
            {
                return 1;
            }
        default:
            {
                return 0;
            }
    }
}

ClientMonster *ClientMonster::create(uint64_t uid, ProcessRun *proc, const ActionNode &action)
{
    switch(const auto monID = uidf::getMonsterID(uid)){
        case DBCOM_MONSTERID(u8"蚂蚁道士"):
            {
                return new ClientAntHealer(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"诺玛法老"):
        case DBCOM_MONSTERID(u8"诺玛大法老"):
            {
                return new ClientNumaWizard(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"红衣法师"):
            {
                return new ClientRedClothWizard(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"洞蛆"):
            {
                return new ClientCaveMaggot(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"火焰沃玛"):
            {
                return new ClientWoomaFlamingWarrior(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"沃玛教主"):
            {
                return new ClientWoomaTaurus(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"暗黑战士"):
            {
                return new ClientDarkWarrior(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"粪虫"):
            {
                return new ClientDung(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"触龙神"):
            {
                return new ClientEvilCentipede(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"祖玛弓箭手"):
            {
                return new ClientZumaArcher(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"祖玛雕像"):
        case DBCOM_MONSTERID(u8"祖玛卫士"):
            {
                return new ClientZumaMonster(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"祖玛教主"):
            {
                return new ClientZumaTaurus(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"爆毒蚂蚁"):
            {
                return new ClientGasAnt(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"沙漠风魔"):
            {
                return new ClientSandEvilFan(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"爆裂蜘蛛"):
            {
                return new ClientBombSpider(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"霸王教主"):
            {
                return new ClientShipwreckLord(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"潘夜左护卫"):
        case DBCOM_MONSTERID(u8"潘夜右护卫"):
            {
                return new ClientMinotaurGuardian(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"变异骷髅"):
            {
                return new ClientTaoSkeleton(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"超强骷髅"):
            {
                return new ClientTaoSkeletonExt(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"神兽"):
            {
                return new ClientTaoDog(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"食人花"):
            {
                return new ClientCannibalPlant(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"角蝇"):
            {
                return new ClientBugbatMaggot(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"沙漠树魔"):
            {
                return new ClientSandCactus(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"掷斧骷髅"):
            {
                return new ClientDualAxeSkeleton(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"楔蛾"):
            {
                return new ClientWedgeMoth(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"沙鬼"):
            {
                return new ClientSandGhost(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"沙漠石人"):
            {
                return new ClientSandStoneMan(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"雷电僵尸"):
            {
                return new ClientLightBoltZombie(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"僧侣僵尸"):
            {
                return new ClientMonkZombie(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"僵尸_1"):
        case DBCOM_MONSTERID(u8"僵尸_2"):
        case DBCOM_MONSTERID(u8"腐僵"):
            {
                return new ClientRebornZombie(uid, proc, action);
            }
        case DBCOM_MONSTERID(u8"栗子树"):
        case DBCOM_MONSTERID(u8"圣诞树"):
        case DBCOM_MONSTERID(u8"圣诞树1"):
            {
                return new ClientTree(uid, proc, action);
            }
        default:
            {
                if(DBCOM_MONSTERRECORD(monID).behaveMode == BM_GUARD){
                    return new ClientGuard(uid, proc, action);
                }
                else{
                    return new ClientMonster(uid, proc, action);
                }
            }
    }
}

// --- merged from clientcannibalplant.cpp ---
bool ClientCannibalPlant::onActionSpawn(const ActionNode &action)
{
    fflassert(m_forcedMotionQueue.empty());
    m_currMotion.reset(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = DIR_BEGIN,
        .x = action.x,
        .y = action.y,
    });

    m_standMode = false;
    return true;
}

bool ClientCannibalPlant::onActionStand(const ActionNode &action)
{
    if(finalStandMode() != to_bool(action.extParam.stand.cannibalPlant.standMode)){
        addActionTransf();
    }
    return true;
}

bool ClientCannibalPlant::onActionTransf(const ActionNode &action)
{
    const auto standReq = to_bool(action.extParam.transf.cannibalPlant.standModeReq);
    if(finalStandMode() != standReq){
        addActionTransf();
    }
    return true;
}

bool ClientCannibalPlant::onActionAttack(const ActionNode &action)
{
    if(!finalStandMode()){
        addActionTransf();
    }

    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = m_processRun->getAimDirection(action, DIR_BEGIN),
        .x = action.x,
        .y = action.y,
    }));
    return true;
}
// --- end clientcannibalplant.cpp ---

// --- merged from clientdualaxeskeleton.cpp ---
ClientDualAxeSkeleton::ClientDualAxeSkeleton(uint64_t uid, ProcessRun *proc, const ActionNode &action)
    : ClientMonster(uid, proc, action)
{
    fflassert(isMonster(u8"掷斧骷髅"));
    switch(action.type){
        case ACTION_SPAWN:
        case ACTION_STAND:
        case ACTION_HITTED:
        case ACTION_DIE:
        case ACTION_ATTACK:
        case ACTION_MOVE:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.type) ? to_d(action.type) : DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });
                break;
            }
        default:
            {
                throw fflpanic("invalid initial action: {}", actionName(action.type));
            }
    }
}

bool ClientDualAxeSkeleton::onActionAttack(const ActionNode &action)
{
    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = m_processRun->getAimDirection(action, currMotion()->direction),
        .x = action.x,
        .y = action.y,
    }));

    m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, this](MotionNode *motionPtr) -> bool
    {
        if(motionPtr->frame < 4){
            return false;
        }

        const auto gfxDirIndex = currMotion()->direction - DIR_BEGIN;
        m_processRun->addFollowUIDMagic(std::unique_ptr<FollowUIDMagic>(new FollowUIDMagic
        {
            u8"掷斧骷髅_掷斧",
            u8"运行",

            currMotion()->x * SYS_MAPGRIDXP,
            currMotion()->y * SYS_MAPGRIDYP,

            gfxDirIndex,
            gfxDirIndex * 2,
            20,

            targetUID,
            m_processRun,
        }))->addOnDone([targetUID, proc = m_processRun](BaseMagic *)
        {
            // TODO interesting bug, don't directly refer to this->m_processRun
            // an dual-axe-skeleton can throw dual-axe-magic and die immediately, which makes *this* dangling

            if(auto coPtr = proc->findUID(targetUID)){
                coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(u8"掷斧骷髅_掷斧", u8"裂解")));
            }
        });
        return true;
    });
    return true;
}
// --- end clientdualaxeskeleton.cpp ---

// --- merged from clientevilcentipede.cpp ---
ClientEvilCentipede::ClientEvilCentipede(uint64_t uid, ProcessRun *proc, const ActionNode &action)
    : ClientStandMonster(uid, proc)
{
    fflassert(isMonster(u8"触龙神"));
    switch(action.type){
        case ACTION_SPAWN:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = false;
                break;
            }
        case ACTION_STAND:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = to_bool(action.extParam.stand.evilCentipede.standMode);
                break;
            }
        case ACTION_ATTACK:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_ATTACK0,
                    .direction = DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = true;
                break;
            }
        case ACTION_TRANSF:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_SPAWN,
                    .direction = DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = to_bool(action.extParam.transf.evilCentipede.standModeReq);
                break;
            }
        case ACTION_HITTED:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_HITTED,
                    .direction = DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = true; // can't be hitted if stay in the soil
                break;
            }
        default:
            {
                throw fflpanic("invalid action: {}", actionName(action));
            }
    }
}

bool ClientEvilCentipede::onActionSpawn(const ActionNode &)
{
    fflassert(m_forcedMotionQueue.empty());
    m_currMotion.reset(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = DIR_BEGIN,
        .x = x(),
        .y = y(),
    });

    m_standMode = false;
    return true;
}

bool ClientEvilCentipede::onActionStand(const ActionNode &action)
{
    if(finalStandMode() != to_bool(action.extParam.stand.evilCentipede.standMode)){
        addActionTransf();
    }
    return true;
}

bool ClientEvilCentipede::onActionTransf(const ActionNode &action)
{
    const auto standReq = to_bool(action.extParam.transf.evilCentipede.standModeReq);
    if(finalStandMode() != standReq){
        addActionTransf();
    }
    return true;
}

bool ClientEvilCentipede::onActionAttack(const ActionNode &)
{
    if(!finalStandMode()){
        addActionTransf();
    }

    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = DIR_BEGIN,
        .x = x(),
        .y = y(),
    }));
    return true;
}

bool ClientEvilCentipede::onActionHitted(const ActionNode &)
{
    if(!finalStandMode()){
        addActionTransf();
    }

    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_HITTED,
        .direction = DIR_BEGIN,
        .x = x(),
        .y = y(),
    }));
    return true;
}
// --- end clientevilcentipede.cpp ---

// --- merged from clientguard.cpp ---
ClientGuard::ClientGuard(uint64_t uid, ProcessRun *proc, const ActionNode &action)
    : ClientMonster(uid, proc)
{
    switch(action.type){
        case ACTION_ATTACK:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_ATTACK0,
                    .direction = m_processRun->getAimDirection(action, DIR_BEGIN),
                    .x = action.x,
                    .y = action.y,
                });
                break;
            }
        default:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.direction) ? action.direction : to_d(DIR_UP),
                    .x = action.x,
                    .y = action.y,
                });
                break;
            }
    }
}

bool ClientGuard::parseAction(const ActionNode &action)
{
    m_lastActive = mirGetTicks();
    m_motionQueue.clear();

    switch(action.type){
        case ACTION_JUMP:
        case ACTION_STAND:
        case ACTION_SPAWN:
            {
                if(action.x != m_currMotion->x || action.y != m_currMotion->y){
                    m_currMotion.reset(new MotionNode
                    {
                        .type = MOTION_MON_STAND,
                        .direction = action.direction,
                        .x = action.x,
                        .y = action.y,
                    });
                }
                return true;
            }
        case ACTION_ATTACK:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_ATTACK0,
                    .direction = m_processRun->getAimDirection(action, m_currMotion->direction),
                    .x = action.x,
                    .y = action.y,
                });
                return true;
            }
        default:
            {
                throw fflvalue(actionName(action.type));
            }
    }
}
// --- end clientguard.cpp ---

// --- merged from clientminotaurguardian.cpp ---
bool ClientMinotaurGuardian::onActionAttack(const ActionNode &action)
{
    const auto [endX, endY, endDir] = motionEndGLoc().at(1);
    m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);

    switch(const auto magicID = action.extParam.attack.magicID){
        case DBCOM_MAGICID(u8"潘夜右护卫_电魔杖"):
        case DBCOM_MAGICID(u8"潘夜左护卫_火魔杖"):
            {
                m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
                {
                    .type = MOTION_MON_ATTACK0,
                    .direction = m_processRun->getAimDirection(action, currMotion()->direction),
                    .x = action.x,
                    .y = action.y,
                }));

                m_motionQueue.back()->effect.reset(new MotionSyncEffect(DBCOM_MAGICRECORD(magicID).name, u8"运行", this, m_motionQueue.back().get(), 3));
                m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, magicID, this](MotionNode *motionPtr) -> bool
                {
                    if(motionPtr->frame < 4){
                        return false;
                    }

                    if(auto coPtr = m_processRun->findUID(targetUID)){
                        coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(DBCOM_MAGICRECORD(magicID).name, u8"裂解")));
                    }
                    return true;
                });
                return true;
            }
        case DBCOM_MAGICID(u8"潘夜右护卫_雷电术"):
            {
                m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
                {
                    .type = MOTION_MON_ATTACK1,
                    .direction = m_processRun->getAimDirection(action, currMotion()->direction),
                    .x = action.x,
                    .y = action.y,
                }));

                m_motionQueue.back()->addTrigger(false, [magicID, targetUID = action.aimUID, this](MotionNode *motionPtr) -> bool
                {
                    if(motionPtr->frame < 4){
                        return false;
                    }

                    if(auto coPtr = m_processRun->findUID(targetUID)){
                        coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new Thunderbolt(DBCOM_MAGICRECORD(magicID).name)));
                    }
                    return true;
                });
                return true;
            }
        case DBCOM_MAGICID(u8"潘夜左护卫_火球术"):
            {
                m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
                {
                    .type = MOTION_MON_ATTACK1,
                    .direction = m_processRun->getAimDirection(action, currMotion()->direction),
                    .x = action.x,
                    .y = action.y,
                }));

                m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, this](MotionNode *motionPtr) -> bool
                {
                    if(motionPtr->frame < 4){
                        return false;
                    }

                    const auto gfx16DirIndex = [targetUID, motionPtr, this]() -> int
                    {
                        if(auto coPtr = m_processRun->findUID(targetUID)){
                            return pathf::getDir16((coPtr->x() - motionPtr->x) * SYS_MAPGRIDXP, (coPtr->y() - motionPtr->y) * SYS_MAPGRIDYP);
                        }
                        return (motionPtr->direction - DIR_BEGIN) * 2;
                    }();

                    m_processRun->addFollowUIDMagic(std::unique_ptr<FollowUIDMagic>(new FollowUIDMagic
                    {
                        u8"潘夜左护卫_火球术",
                        u8"运行",

                        motionPtr->x * SYS_MAPGRIDXP,
                        motionPtr->y * SYS_MAPGRIDYP,

                        gfx16DirIndex,
                        gfx16DirIndex,
                        20,

                        targetUID,
                        m_processRun,
                    }))->addOnDone([targetUID, proc = m_processRun](BaseMagic *)
                    {
                        if(auto coPtr = proc->findUID(targetUID)){
                            coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(u8"潘夜左护卫_火球术", u8"裂解")));
                        }
                    });
                    return true;
                });
                return true;
            }
        default:
            {
                throw fflpanic("invalid DC: id = {}, name = {}", magicID, to_cstr(DBCOM_MAGICRECORD(magicID).name));
            }
    }
}
// --- end clientminotaurguardian.cpp ---

// --- merged from clientnumawizard.cpp ---
bool ClientNumaWizard::onActionAttack_fireBall(const ActionNode &action)
{
    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = m_processRun->getAimDirection(action, currMotion()->direction),
        .x = action.x,
        .y = action.y,
    }));

    m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, this](MotionNode *motionPtr) -> bool
    {
        if(motionPtr->frame < 4){
            return false;
        }

        const auto gfx16DirIndex = [targetUID, motionPtr, this]() -> int
        {
            if(auto coPtr = m_processRun->findUID(targetUID)){
                return pathf::getDir16((coPtr->x() - motionPtr->x) * SYS_MAPGRIDXP, (coPtr->y() - motionPtr->y) * SYS_MAPGRIDYP);
            }
            return (motionPtr->direction - DIR_BEGIN) * 2;
        }();

        m_processRun->addFollowUIDMagic(std::unique_ptr<FollowUIDMagic>(new FollowUIDMagic
        {
            u8"诺玛法老_火球术",
            u8"运行",

            motionPtr->x * SYS_MAPGRIDXP,
            motionPtr->y * SYS_MAPGRIDYP,

            gfx16DirIndex,
            gfx16DirIndex,
            20,

            targetUID,
            m_processRun,
        }))->addOnDone([targetUID, proc = m_processRun](BaseMagic *)
        {
            if(auto coPtr = proc->findUID(targetUID)){
                coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(u8"诺玛法老_火球术", u8"裂解")));
            }
        });
        return true;
    });
    return true;
}

bool ClientNumaWizard::onActionAttack_thunderBolt(const ActionNode &action)
{
    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = m_processRun->getAimDirection(action, currMotion()->direction),
        .x = action.x,
        .y = action.y,
    }));

    m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, this](MotionNode *motionPtr) -> bool
    {
        if(motionPtr->frame < 5){
            return false;
        }

        if(auto coPtr = m_processRun->findUID(targetUID)){
            coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new Thunderbolt()));
        }
        return true;
    });
    return true;
}
// --- end clientnumawizard.cpp ---

// --- merged from clientrebornzombie.cpp ---
bool ClientRebornZombie::onActionSpawn(const ActionNode &action)
{
    fflassert(m_forcedMotionQueue.empty());
    m_currMotion.reset(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
        .x = action.x,
        .y = action.y,
    });

    m_standMode = false;
    return true;
}

bool ClientRebornZombie::onActionStand(const ActionNode &action)
{
    if(finalStandMode() != to_bool(action.extParam.stand.sandGhost.standMode)){
        addActionTransf();
    }
    return true;
}

bool ClientRebornZombie::onActionTransf(const ActionNode &action)
{
    const auto standReq = to_bool(action.extParam.transf.sandGhost.standModeReq);
    if(finalStandMode() != standReq){
        addActionTransf();
    }
    return true;
}

bool ClientRebornZombie::onActionAttack(const ActionNode &action)
{
    if(!finalStandMode()){
        addActionTransf();
    }

    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = m_processRun->getAimDirection(action, currMotion()->direction),
        .x = action.x,
        .y = action.y,
    }));
    return true;
}
// --- end clientrebornzombie.cpp ---

// --- merged from clientsandcactus.cpp ---
ClientSandCactus::ClientSandCactus(uint64_t uid, ProcessRun *proc, const ActionNode &action)
    : ClientMonster(uid, proc, action)
{
    fflassert(isMonster(u8"沙漠树魔"));
    switch(action.type){
        case ACTION_SPAWN:
        case ACTION_STAND:
        case ACTION_HITTED:
        case ACTION_DIE:
        case ACTION_ATTACK:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.type) ? to_d(action.type) : DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });
                break;
            }
        default:
            {
                throw fflpanic("Taodog get invalid initial action: {}", actionName(action.type));
            }
    }
}

bool ClientSandCactus::onActionAttack(const ActionNode &action)
{
    fflassert(action.x == currMotion()->x);
    fflassert(action.y == currMotion()->y);

    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = m_processRun->getAimDirection(action, currMotion()->direction),
        .x = action.x,
        .y = action.y,
    }));

    m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, this](MotionNode *motionPtr) -> bool
    {
        if(motionPtr->frame < 5){
            return false;
        }

        m_processRun->addFollowUIDMagic(std::unique_ptr<FollowUIDMagic>(new FollowUIDMagic
        {
            u8"沙漠树魔_喷刺",
            u8"运行",

            currMotion()->x * SYS_MAPGRIDXP,
            currMotion()->y * SYS_MAPGRIDYP,

            0,
            (m_currMotion->direction - DIR_BEGIN) * 2,
            20,

            targetUID,
            m_processRun,
        }))->addOnDone([targetUID, proc = m_processRun](BaseMagic *)
        {
            if(auto coPtr = proc->findUID(targetUID)){
                coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(u8"沙漠树魔_喷刺", u8"裂解")));
            }
        });
        return true;
    });
    return true;
}
// --- end clientsandcactus.cpp ---

// --- merged from clientsandghost.cpp ---
bool ClientSandGhost::onActionSpawn(const ActionNode &action)
{
    fflassert(m_forcedMotionQueue.empty());
    m_currMotion.reset(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
        .x = action.x,
        .y = action.y,
    });

    m_standMode = false;
    return true;
}

bool ClientSandGhost::onActionStand(const ActionNode &action)
{
    if(finalStandMode() != to_bool(action.extParam.stand.sandGhost.standMode)){
        addActionTransf();
    }
    return true;
}

bool ClientSandGhost::onActionTransf(const ActionNode &action)
{
    const auto standReq = to_bool(action.extParam.transf.sandGhost.standModeReq);
    if(finalStandMode() != standReq){
        addActionTransf();
    }
    return true;
}

bool ClientSandGhost::onActionAttack(const ActionNode &action)
{
    if(!finalStandMode()){
        addActionTransf();
    }

    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = m_processRun->getAimDirection(action, currMotion()->direction),
        .x = action.x,
        .y = action.y,
    }));
    return true;
}
// --- end clientsandghost.cpp ---

// --- merged from clientshipwrecklord.cpp ---
bool ClientShipwreckLord::onActionAttack(const ActionNode &action)
{
    const auto [endX, endY, endDir] = motionEndGLoc().at(1);
    m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);

    switch(const auto magicID = action.extParam.attack.magicID){
        case DBCOM_MAGICID(u8"物理攻击"):
            {
                m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
                {
                    .type = MOTION_MON_ATTACK0,
                    .direction = m_processRun->getAimDirection(action, currMotion()->direction),
                    .x = action.x,
                    .y = action.y,
                }));

                m_motionQueue.back()->effect.reset(new MotionSyncEffect(u8"霸王教主_火刃", u8"运行", this, m_motionQueue.back().get()));
                return true;
            }
        case DBCOM_MAGICID(u8"霸王教主_野蛮冲撞"):
            {
                m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
                {
                    .type = MOTION_MON_SPELL0,
                    .direction = m_processRun->getAimDirection(action, currMotion()->direction),
                    .x = action.x,
                    .y = action.y,
                }));
                return true;
            }
        default:
            {
                throw fflpanic("invalid DC: id = {}, name = {}", magicID, to_cstr(DBCOM_MAGICRECORD(magicID).name));
            }
    }
}
// --- end clientshipwrecklord.cpp ---

// --- merged from clienttaodog.cpp ---
ClientTaoDog::ClientTaoDog(uint64_t uid, ProcessRun *proc, const ActionNode &action)
    : ClientStandMonster(uid, proc)
{
    fflassert(isMonster(u8"神兽"));
    switch(action.type){
        case ACTION_SPAWN:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_SPECIAL,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = false;
                break;
            }
        case ACTION_STAND:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = action.extParam.stand.dog.standMode;
                break;
            }
        case ACTION_HITTED:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_HITTED,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = action.extParam.hitted.dog.standMode;
                break;
            }
        case ACTION_DIE:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_DIE,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = action.extParam.die.dog.standMode;
                break;
            }
        case ACTION_ATTACK:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_ATTACK0,
                    .direction = m_processRun->getAimDirection(action, DIR_UP),
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = true;
                break;
            }
        case ACTION_MOVE:
        case ACTION_SPACEMOVE:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.aimX,
                    .y = action.aimY,
                });

                // TODO use crowling state
                //      next ACTION_STAND/ACTION_MOVE will fix it immdiately
                m_standMode = false;
                break;
            }
        default:
            {
                throw fflpanic("Taodog get invalid initial action: {}", actionName(action.type));
            }
    }
}

bool ClientTaoDog::onActionStand(const ActionNode &action)
{
    if(finalStandMode() != to_bool(action.extParam.stand.dog.standMode)){
        addActionTransf();
    }
    return ClientMonster::onActionStand(action);
}

bool ClientTaoDog::onActionSpawn(const ActionNode &action)
{
    fflassert(m_forcedMotionQueue.empty());
    m_currMotion.reset(new MotionNode
    {
        .type = MOTION_MON_SPAWN,
        .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
        .x = action.x,
        .y = action.y,
    });

    m_standMode = false;
    return true;
}

bool ClientTaoDog::onActionTransf(const ActionNode &action)
{
    const auto standReq = to_bool(action.extParam.transf.dog.standModeReq);
    if(finalStandMode() != standReq){
        addActionTransf();
    }
    return true;
}

bool ClientTaoDog::onActionAttack(const ActionNode &action)
{
    if(!finalStandMode()){
        addActionTransf();
    }

    const auto [endX, endY, endDir] = motionEndGLoc().at(1);
    m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = m_processRun->getAimDirection(action, endDir),
        .x = action.x,
        .y = action.y,
    }));

    m_motionQueue.back()->addTrigger(false, [this](MotionNode *motionPtr) -> bool
    {
        if(motionPtr->frame < 5){
            return false;
        }

        fflassert(m_standMode);
        m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FixedLocMagic
        {
            u8"神兽_喷火",
            u8"运行",
            currMotion()->x,
            currMotion()->y,
            currMotion()->direction - DIR_BEGIN,
        }));
        return true;
    });
    return true;
}
// --- end clienttaodog.cpp ---

// --- merged from clientwedgemoth.cpp ---
ClientWedgeMoth::ClientWedgeMoth(uint64_t uid, ProcessRun *proc, const ActionNode &action)
    : ClientMonster(uid, proc, action)
{
    fflassert(isMonster(u8"楔蛾"));
    switch(action.type){
        case ACTION_SPAWN:
        case ACTION_STAND:
        case ACTION_HITTED:
        case ACTION_DIE:
        case ACTION_ATTACK:
        case ACTION_MOVE:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.type) ? to_d(action.type) : DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });
                break;
            }
        default:
            {
                throw fflpanic("invalid initial action: {}", actionName(action.type));
            }
    }
}

bool ClientWedgeMoth::onActionAttack(const ActionNode &action)
{
    m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_ATTACK0,
        .direction = m_processRun->getAimDirection(action, currMotion()->direction),
        .x = action.x,
        .y = action.y,
    }));

    m_motionQueue.back()->addTrigger(false, [this](MotionNode *motionPtr) -> bool
    {
        if(motionPtr->frame < 5){
            return false;
        }

        m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FixedLocMagic
        {
            u8"楔蛾_喷毒",
            u8"运行",
            currMotion()->x,
            currMotion()->y,
            currMotion()->direction - DIR_BEGIN,
        }));
        return true;
    });
    return true;
}
// --- end clientwedgemoth.cpp ---

// --- merged from clientzumamonster.cpp ---
ClientZumaMonster::ClientZumaMonster(uint64_t uid, ProcessRun *proc, const ActionNode &action)
    : ClientStandMonster(uid, proc)
{
    fflassert(isMonster(u8"祖玛雕像") || isMonster(u8"祖玛卫士"));
    switch(action.type){
        case ACTION_SPAWN:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = false;
                break;
            }
        case ACTION_STAND:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = to_bool(action.extParam.stand.zumaMonster.standMode);
                break;
            }
        case ACTION_ATTACK:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_ATTACK0,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = true;
                break;
            }
        case ACTION_MOVE:
            {
                // use MOTION_MON_STAND
                // MOTION_MON_WALK needs to figure the destination grid

                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = true;
                break;
            }
        case ACTION_TRANSF:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_SPAWN,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = to_bool(action.extParam.transf.zumaMonster.standModeReq);
                break;
            }
        case ACTION_HITTED:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_HITTED,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = true; // can't be hitted if stay in the soil
                break;
            }
        default:
            {
                throw fflpanic("invalid action: {}", actionName(action));
            }
    }
}

bool ClientZumaMonster::onActionSpawn(const ActionNode &action)
{
    fflassert(m_forcedMotionQueue.empty());
    m_currMotion.reset(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
        .x = action.x,
        .y = action.y,
    });

    m_standMode = false;
    return true;
}

bool ClientZumaMonster::onActionStand(const ActionNode &action)
{
    if(finalStandMode() != to_bool(action.extParam.stand.zumaMonster.standMode)){
        addActionTransf();
    }
    return true;
}

bool ClientZumaMonster::onActionTransf(const ActionNode &action)
{
    const auto standReq = to_bool(action.extParam.transf.zumaMonster.standModeReq);
    if(finalStandMode() != standReq){
        addActionTransf();
    }
    return true;
}

bool ClientZumaMonster::onActionAttack(const ActionNode &action)
{
    if(!finalStandMode()){
        addActionTransf();
    }
    return ClientMonster::onActionAttack(action);
}
// --- end clientzumamonster.cpp ---

// --- merged from clientzumataurus.cpp ---
static std::unique_ptr<MotionNode> fnMakeStandMotion(int x, int y)
{
    return std::unique_ptr<MotionNode>(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = DIR_DOWNLEFT,
        .x = x,
        .y = y,
    });
}

ClientZumaTaurus::ClientZumaTaurus(uint64_t uid, ProcessRun *proc, const ActionNode &action)
    : ClientStandMonster(uid, proc)
{
    fflassert(isMonster(u8"祖玛教主"));
    switch(action.type){
        case ACTION_SPAWN:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_STAND,
                    .direction = DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = false;
                break;
            }
        case ACTION_STAND:
            {
                if(action.extParam.stand.zumaTaurus.standMode){
                    m_currMotion.reset(new MotionNode
                    {
                        .type = MOTION_MON_STAND,
                        .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                        .x = action.x,
                        .y = action.y,
                    });
                    m_standMode = true;
                }
                else{
                    m_currMotion.reset(new MotionNode
                    {
                        .type = MOTION_MON_STAND,
                        .direction = DIR_BEGIN,
                        .x = action.x,
                        .y = action.y,
                    });
                    m_standMode = false;
                }
                break;
            }
        case ACTION_ATTACK:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_ATTACK0,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = true;
                break;
            }
        case ACTION_MOVE:
            {
                m_currMotion.reset(new MotionNode
                {
                    // use STAND
                    // otherwise need to figure out proper (endX, endY)
                    .type = MOTION_MON_STAND,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = true;
                break;
            }
        case ACTION_TRANSF:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_SPAWN,
                    .direction = DIR_BEGIN,
                    .x = action.x,
                    .y = action.y,
                });

                m_currMotion->addTrigger(false, [this](MotionNode *motionPtr) -> bool
                {
                    if(motionPtr->frame < 9){
                        return false;
                    }

                    m_forcedMotionQueue.push_back(fnMakeStandMotion(motionPtr->x, motionPtr->y));

                    m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new ZumaTaurusFragmentEffect_RUN(
                        motionPtr->x,
                        motionPtr->y)));
                    return true;
                });

                m_standMode = true;
                break;
            }
        case ACTION_HITTED:
            {
                m_currMotion.reset(new MotionNode
                {
                    .type = MOTION_MON_HITTED,
                    .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                    .x = action.x,
                    .y = action.y,
                });

                m_standMode = true; // can't be hitted if stay in the soil
                break;
            }
        default:
            {
                throw fflpanic("invalid action: {}", actionName(action));
            }
    }
}

bool ClientZumaTaurus::onActionSpawn(const ActionNode &action)
{
    fflassert(m_forcedMotionQueue.empty());
    m_currMotion.reset(new MotionNode
    {
        .type = MOTION_MON_STAND,
        .direction = DIR_BEGIN,
        .x = action.x,
        .y = action.y,
    });

    m_standMode = false;
    return true;
}

bool ClientZumaTaurus::onActionStand(const ActionNode &action)
{
    if(finalStandMode() != to_bool(action.extParam.stand.zumaTaurus.standMode)){
        addActionTransf();
    }
    return true;
}

bool ClientZumaTaurus::onActionTransf(const ActionNode &action)
{
    const auto standReq = to_bool(action.extParam.transf.zumaTaurus.standModeReq);
    if(finalStandMode() != standReq){
        addActionTransf();
    }
    return true;
}

bool ClientZumaTaurus::onActionAttack(const ActionNode &action)
{
    if(!finalStandMode()){
        addActionTransf();
    }

    const auto [endX, endY, endDir] = motionEndGLoc().at(1);
    m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
    if(auto coPtr = m_processRun->findUID(action.aimUID)){
        m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
        {
            .type = MOTION_MON_ATTACK0,
            .direction = [&action, endDir, coPtr]() -> int
            {
                const auto nX = coPtr->x();
                const auto nY = coPtr->y();
                if(mathf::LDistance2<int>(nX, nY, action.x, action.y) == 0){
                    return endDir;
                }
                return pathf::getOffDir(action.x, action.y, nX, nY);
            }(),
            .x = action.x,
            .y = action.y,
        }));

        switch(action.extParam.attack.magicID){
            case DBCOM_MAGICID(u8"祖玛教主_火墙"):
                {
                    m_motionQueue.back()->effect = std::unique_ptr<MotionAlignedEffect>(new MotionAlignedEffect
                    {
                        u8"祖玛教主_火墙",
                        u8"启动",
                        this,
                        m_motionQueue.back().get(),
                    });
                    break;
                }
            case DBCOM_MAGICID(u8"祖玛教主_地狱火"):
                {
                    m_motionQueue.back()->effect = std::unique_ptr<MotionAlignedEffect>(new MotionAlignedEffect
                    {
                        u8"祖玛教主_地狱火",
                        u8"启动",
                        this,
                        m_motionQueue.back().get(),
                    });

                    m_motionQueue.back()->addTrigger(false, [action, this](MotionNode *motionPtr) -> bool
                    {
                        if(motionPtr->frame < 4){
                            return false;
                        }

                        const auto standDir = [motionPtr, &action, this]() -> int
                        {
                            if(action.aimUID){
                                if(auto coPtr = m_processRun->findUID(action.aimUID); coPtr && coPtr->getTargetBox()){
                                    if(const auto dir = m_processRun->getAimDirection(action, DIR_NONE); dir != DIR_NONE){
                                        return dir;
                                    }
                                }
                            }
                            return motionPtr->direction;
                        }();

                        const auto castX = motionPtr->endX;
                        const auto castY = motionPtr->endY;

                        for(const auto distance: {1, 2, 3, 4, 5, 6, 7, 8}){
                            m_processRun->addDelay(distance * 100, [standDir, castX, castY, distance, castMapID = m_processRun->mapID(), proc = m_processRun]()
                            {
                                if(proc->mapID() != castMapID){
                                    return;
                                }

                                const auto [aimX, aimY] = pathf::getFrontGLoc(castX, castY, standDir, distance);
                                if(!proc->groundValid(aimX, aimY)){
                                    return;
                                }

                                proc->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new HellFire_RUN
                                {
                                    aimX,
                                    aimY,
                                    standDir,
                                }))->addTrigger([aimX, aimY, proc](BaseMagic *magicPtr)
                                {
                                    if(magicPtr->frame() < 10){
                                        return false;
                                    }

                                    proc->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FireAshEffect_RUN
                                    {
                                        aimX,
                                        aimY,
                                        1000,
                                    }));
                                    return true;
                                });
                            });
                        }
                        return true;
                    });
                    break;
                }
            default:
                {
                    throw fflreach();
                }
        }
    }
    return true;
}

void ClientZumaTaurus::addActionTransf()
{
    ClientStandMonster::addActionTransf();

    fflassert(!m_forcedMotionQueue.empty());
    fflassert(m_forcedMotionQueue.back()->type == MOTION_MON_SPAWN);

    m_forcedMotionQueue.back()->addTrigger(false, [this](MotionNode *motionPtr) -> bool
    {
        if(motionPtr->frame < 9){
            return false;
        }

        m_forcedMotionQueue.push_back(fnMakeStandMotion(motionPtr->x, motionPtr->y));

        m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new ZumaTaurusFragmentEffect_RUN(motionPtr->x, motionPtr->y)));
        return true;
    });
}
// --- end clientzumataurus.cpp ---
