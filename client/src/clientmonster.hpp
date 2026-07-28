#pragma once
#include <optional>
#include "uidf.hpp"
#include "pathf.hpp"
#include "client.hpp"
#include "clientmsg.hpp"
#include "protocoldef.hpp"
#include "creaturemovable.hpp"
#include <cstdint>
#include <unordered_map>
#include "actionnode.hpp"
#include "dbcomid.hpp"
#include "fflerror.hpp"
#include "fixedlocmagic.hpp"
#include "motion.hpp"
#include "processrun.hpp"
#include "totype.hpp"

class ClientMonster;
struct MonsterFrameGfxSeq final
{
    const std::optional<int>      gfxLookID {};
    const std::optional<int>    gfxMotionID {};
    const std::optional<int> gfxDirectionID {};

    const int begin = 0;
    const int count = 0;
    const bool reverse = false;

    operator bool() const
    {
        return begin >= 0 && count > 0;
    }

    std::optional<uint32_t> gfxID(const ClientMonster *, std::optional<int> = {}) const;
};

class ClientMonster: public CreatureMovable
{
    public:
        ClientMonster(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc)
        {
            switch(action.type){
                case ACTION_DIE:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_DIE,
                            .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                            .x = action.x,
                            .y = action.y,
                        });

                        if(const auto deathEffectName = str_printf(u8"%s_死亡特效", to_cstr(monsterName())); DBCOM_MAGICID(to_u8cstr(deathEffectName))){
                            m_currMotion->addTrigger(true, [deathEffectName, this](MotionNode *) -> bool
                            {
                                addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(to_u8cstr(deathEffectName), u8"运行")));
                                return true;
                            });
                        }
                        break;
                    }
                default:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_STAND,
                            .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                            .x = action.x,
                            .y = action.y,
                        });
                        break;
                    }
            }
        }

    protected:
        ClientMonster(uint64_t, ProcessRun *);

    public:
        bool update(double) override;

    public:
        void drawFrame(int, int, int, int, bool) override;

    public:
        uint32_t monsterID() const
        {
            return uidf::getMonsterID(UID());
        }

        std::u8string_view monsterName() const
        {
            return DBCOM_MONSTERRECORD(monsterID()).name;
        }

        const auto &getMR() const
        {
            return DBCOM_MONSTERRECORD(monsterID());
        }

        int lookID() const
        {
            return getMR().lookID;
        }

    public:
        uint32_t getSeffID(int) const;

    public:
        int getFrameCount(const MotionNode *motionPtr) const override
        {
            return getFrameGfxSeq(motionPtr->type, motionPtr->direction).count;
        }

    public:
        bool parseAction(const ActionNode &) override;

    public:
        bool motionValid(const std::unique_ptr<MotionNode> &) const override;

    public:
        virtual MonsterFrameGfxSeq getFrameGfxSeq(int, int) const;

    protected:
        std::unique_ptr<MotionNode> makeWalkMotion(int, int, int, int, int) const;

    public:
        int  maxStep() const override;
        int currStep() const override;

    protected:
        virtual bool onActionDie      (const ActionNode &);
        virtual bool onActionStand    (const ActionNode &);
        virtual bool onActionHitted   (const ActionNode &);
        virtual bool onActionJump     (const ActionNode &);
        virtual bool onActionMove     (const ActionNode &);
        virtual bool onActionAttack   (const ActionNode &);
        virtual bool onActionSpawn    (const ActionNode &);
        virtual bool onActionTransf   (const ActionNode &);
        virtual bool onActionSpaceMove(const ActionNode &);

    public:
        ClientCreature::TargetBox getTargetBox() const override;

    protected:
        std::unique_ptr<MotionNode> makeIdleMotion() const override
        {
            return std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_STAND,
                .direction = m_currMotion->direction,
                .x = m_currMotion->endX,
                .y = m_currMotion->endY,
            });
        }

    public:
        bool deadFadeOut() override;

    public:
        static ClientMonster *create(uint64_t, ProcessRun *, const ActionNode &);
};

// --- merged from clientstandmonster.hpp ---
// base class for monsters has stand/lying state
// use MOTION_MON_SPAWN to show the transfer gfx, may need gfx redirection

class ClientStandMonster: public ClientMonster
{
    protected:
        bool m_standMode = false;

    public:
        ClientStandMonster(uint64_t uid, ProcessRun *proc)
            : ClientMonster(uid, proc)
        {}

    protected:
        virtual void addActionTransf()
        {
            const auto [endX, endY, endDir] = motionEndGLoc().at(1);
            m_forcedMotionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_SPAWN,
                .direction = endDir,
                .x = endX,
                .y = endY,
            }));

            m_forcedMotionQueue.back()->addTrigger(true, [this](MotionNode *) -> bool
            {
                m_standMode = !m_standMode;
                return true;
            });
        }

    protected:
        bool finalStandMode() const
        {
            // don't need to count current status
            // if current status is MOTION_MON_SPAWN then the m_standMode has already changed
            //
            // the general rule is: we use end frame status as current status
            // i.e. there is a flower bloom animation, then the m_currMotion->type for this whole animation is "BLOOMED"

            // if(m_currMotion->motion == MOTION_MON_SPAWN){
            //     countTransf++;
            // }

            int countTransf = 0;
            for(const auto &motionPtr: m_forcedMotionQueue){
                if(motionPtr->type == MOTION_MON_SPAWN){
                    countTransf++;
                }
            }
            return to_bool(countTransf % 2) ? !m_standMode : m_standMode;
        }
};
// --- end clientstandmonster.hpp ---

// --- merged from clientanthealer.hpp ---
class ClientAntHealer: public ClientMonster
{
    public:
        ClientAntHealer(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"蚂蚁道士"));
        }

    public:
        bool onActionAttack(const ActionNode &action) override
        {
            const auto [endX, endY, endDir] = motionEndGLoc().at(1);
            m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
            m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_ATTACK0,
                .direction = m_processRun->getAimDirection(action, endDir),
                .x = action.x,
                .y = action.y,
            }));

            m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, proc = m_processRun](MotionNode *motionPtr) -> bool
            {
                if(motionPtr->frame < 3){
                    return false;
                }

                if(auto coPtr = proc->findUID(targetUID)){
                    coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new AntHealing()));
                }
                return true;
            });
            return true;
        }
};
// --- end clientanthealer.hpp ---

// --- merged from clientbombspider.hpp ---
class ClientBombSpider: public ClientMonster
{
    public:
        ClientBombSpider(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"爆裂蜘蛛"));
        }

        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int dir) const override
        {
            switch(motion){
                case MOTION_MON_STAND:
                    {
                        // 爆裂蜘蛛 would never stop moving
                        // but server can still post ACTION_STAND for position-sync

                        return
                        {
                            .gfxMotionID = MOTION_MON_WALK,
                            .begin = 0,
                            .count = 1,
                        };
                    }
                case MOTION_MON_WALK:
                case MOTION_MON_DIE:
                    {
                        return ClientMonster::getFrameGfxSeq(motion, dir);
                    }
                default:
                    {
                        return {};
                    }
            }
        }
};
// --- end clientbombspider.hpp ---

// --- merged from clientbugbatmaggot.hpp ---
class ClientBugbatMaggot: public ClientMonster
{
    public:
        ClientBugbatMaggot(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc)
        {
            fflassert(isMonster(u8"角蝇"));
            m_currMotion.reset(new MotionNode
            {
                .type = MOTION_MON_STAND,
                .direction = DIR_BEGIN,
                .x = action.x,
                .y = action.y,
            });
        }

    protected:
        bool onActionAttack(const ActionNode &) override
        {
            m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_ATTACK0,
                .direction = DIR_BEGIN,
                .x = m_currMotion->endX,
                .y = m_currMotion->endY,
            }));
            return true;
        }
};
// --- end clientbugbatmaggot.hpp ---

// --- merged from clientcannibalplant.hpp ---
class ClientCannibalPlant: public ClientStandMonster
{
    public:
        ClientCannibalPlant(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientStandMonster(uid, proc)
        {
            fflassert(isMonster(u8"食人花"));
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

                        m_standMode = to_bool(action.extParam.stand.cannibalPlant.standMode);
                        break;
                    }
                case ACTION_ATTACK:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_ATTACK0,
                            .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_BEGIN,
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

                        m_standMode = to_bool(action.extParam.transf.cannibalPlant.standModeReq);
                        break;
                    }
                case ACTION_HITTED:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_HITTED,
                            .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_BEGIN,
                            .x = action.x,
                            .y = action.y,
                        });

                        m_standMode = true; // can't be hitted if stay in the soil
                        break;
                    }
                default:
                    {
                        throw fflreach();
                    }
            }
        }

    public:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int direction) const override
        {
            if(m_standMode){
                switch(motion){
                    case MOTION_MON_STAND:
                        {
                            if(direction == DIR_BEGIN){
                                return {.count = 4};
                            }
                            return {};
                        }
                    case MOTION_MON_SPAWN:
                        {
                            return
                            {
                                .begin = 7,
                                .count = 8,
                                .reverse = true,
                            };
                        }
                    case MOTION_MON_ATTACK0: return {.count = 6};
                    case MOTION_MON_HITTED : return {.count = 2};
                    case MOTION_MON_DIE    : return {.count =10};
                    default                : return {};
                }
            }
            else{
                switch(motion){
                    case MOTION_MON_STAND:
                        {
                            if(direction == DIR_BEGIN){
                                return
                                {
                                    .gfxMotionID = MOTION_MON_SPAWN,
                                    .begin = 7,
                                    .count = 1,
                                };
                            }
                            return {};
                        }
                    case MOTION_MON_SPAWN:
                        {
                            if(direction == DIR_BEGIN){
                                return
                                {
                                    .begin = 0,
                                    .count = 8,
                                };
                            }
                            return {};
                        }
                    default:
                        {
                            return {};
                        }
                }
            }
        }

    protected:
        bool onActionSpawn (const ActionNode &) override;
        bool onActionStand (const ActionNode &) override;
        bool onActionTransf(const ActionNode &) override;
        bool onActionAttack(const ActionNode &) override;

    public:
        bool canFocus(int pointX, int pointY) const override
        {
            return ClientCreature::canFocus(pointX, pointY) && m_standMode;
        }

    protected:
        std::unique_ptr<MotionNode> makeIdleMotion() const override
        {
            return std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_STAND,
                .direction = DIR_BEGIN,
                .x = m_currMotion->endX,
                .y = m_currMotion->endY,
            });
        }
};
// --- end clientcannibalplant.hpp ---

// --- merged from clientcavemaggot.hpp ---
class ClientCaveMaggot: public ClientMonster
{
    public:
        ClientCaveMaggot(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"洞蛆"));
        }

    protected:
        bool onActionAttack(const ActionNode &action)
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
                if(motionPtr->frame < 3){
                    return false;
                }

                m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FixedLocMagic
                {
                    u8"洞蛆_喷毒",
                    u8"运行",
                    currMotion()->x,
                    currMotion()->y,
                    currMotion()->direction - DIR_BEGIN,
                }));
                return true;
            });
            return true;
        }
};
// --- end clientcavemaggot.hpp ---

// --- merged from clientdarkwarrior.hpp ---
class ClientDarkWarrior: public ClientMonster
{
    public:
        ClientDarkWarrior(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"暗黑战士"));
        }

    protected:
        bool onActionAttack(const ActionNode &action) override
        {
            const auto [endX, endY, endDir] = motionEndGLoc().at(1);
            m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
            m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_ATTACK0,
                .direction = m_processRun->getAimDirection(action, endDir),
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
                    u8"暗黑战士_喷刺",
                    u8"运行",

                    currMotion()->x * SYS_MAPGRIDXP,
                    currMotion()->y * SYS_MAPGRIDYP,

                    (m_currMotion->direction - DIR_BEGIN) * 2,
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
};
// --- end clientdarkwarrior.hpp ---

// --- merged from clientdualaxeskeleton.hpp ---
class ClientDualAxeSkeleton: public ClientMonster
{
    public:
        ClientDualAxeSkeleton(uint64_t, ProcessRun *, const ActionNode &);

    protected:
        bool onActionAttack(const ActionNode &) override;
};
// --- end clientdualaxeskeleton.hpp ---

// --- merged from clientdung.hpp ---
class ClientDung: public ClientMonster
{
    public:
        ClientDung(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"粪虫"));
        }

    protected:
        bool onActionAttack(const ActionNode &action)
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
                if(motionPtr->frame < 3){
                    return false;
                }

                m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FixedLocMagic
                {
                    u8"粪虫_喷毒",
                    u8"运行",
                    currMotion()->x,
                    currMotion()->y,
                    currMotion()->direction - DIR_BEGIN,
                }));
                return true;
            });
            return true;
        }
};
// --- end clientdung.hpp ---

// --- merged from clientevilcentipede.hpp ---
class ClientEvilCentipede: public ClientStandMonster
{
    public:
        ClientEvilCentipede(uint64_t, ProcessRun *, const ActionNode &);

    public:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int) const override
        {
            // index frames   gfx
            // 07000   4      stand
            // 07160   6      AOE, attach far
            // 07170   6      attack close, middle
            // 07180   6      attack close, left
            // 07190   6      attack close, right
            // 07240   2      hitted
            // 07320  10      die
            // 07640  10      hide/appear

            if(m_standMode){
                switch(motion){
                    case MOTION_MON_STAND  : return {.count =  4};
                    case MOTION_MON_ATTACK0: return {.count =  6};
                    case MOTION_MON_HITTED : return {.count =  2};
                    case MOTION_MON_SPAWN : return {.count = 10};
                    default                : return {};
                }
            }
            else{
                switch(motion){
                    case MOTION_MON_STAND:
                        {
                            return
                            {
                                .gfxMotionID = MOTION_MON_SPAWN,
                                .begin = 0,
                                .count = 1,
                            };
                        }
                    case MOTION_MON_SPAWN:
                        {
                            return
                            {
                                .gfxMotionID = MOTION_MON_SPAWN,
                                .begin   =  9,
                                .count   = 10,
                                .reverse = true,
                            };
                        }
                    default:
                        {
                            return {};
                        }
                }
            }
        }

    protected:
        bool onActionSpawn (const ActionNode &) override;
        bool onActionStand (const ActionNode &) override;
        bool onActionTransf(const ActionNode &) override;
        bool onActionAttack(const ActionNode &) override;
        bool onActionHitted(const ActionNode &) override;

    public:
        bool canFocus(int pointX, int pointY) const override
        {
            return ClientCreature::canFocus(pointX, pointY) && m_standMode;
        }
};
// --- end clientevilcentipede.hpp ---

// --- merged from clientgasant.hpp ---
class ClientGasAnt: public ClientMonster
{
    public:
        ClientGasAnt(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"爆毒蚂蚁"));
        }

    protected:
        bool onActionAttack(const ActionNode &action) override
        {
            const auto [endX, endY, endDir] = motionEndGLoc().at(1);
            m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
            m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_ATTACK0,
                .direction = m_processRun->getAimDirection(action, currMotion()->direction),
                .x = action.x,
                .y = action.y,
            }));

            m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, this](MotionNode *motionPtr) -> bool
            {
                if(motionPtr->frame < 2){
                    return false;
                }

                m_processRun->addFollowUIDMagic(std::unique_ptr<FollowUIDMagic>(new FollowUIDMagic
                {
                    u8"爆毒蚂蚁_喷毒",
                    u8"运行",

                    motionPtr->x * SYS_MAPGRIDXP,
                    motionPtr->y * SYS_MAPGRIDYP,

                    0,
                    (motionPtr->direction - DIR_BEGIN) * 2,
                    20,

                    targetUID,
                    m_processRun,
                }));
                return true;
            });
            return true;
        }
};
// --- end clientgasant.hpp ---

// --- merged from clientguard.hpp ---
class ClientGuard: public ClientMonster
{
    public:
        ClientGuard(uint64_t, ProcessRun *, const ActionNode &);

    public:
        bool parseAction(const ActionNode &) override;
};
// --- end clientguard.hpp ---

// --- merged from clientlightboltzombie.hpp ---
class ClientLightBoltZombie: public ClientMonster
{
    public:
        ClientLightBoltZombie(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"雷电僵尸"));
            switch(action.type){
                case ACTION_DIE:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_DIE,
                            .direction = pathf::dirValid(action.type) ? to_d(action.type) : DIR_UP,
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
                            .direction = pathf::dirValid(action.type) ? to_d(action.type) : DIR_UP,
                            .x = action.x,
                            .y = action.y,
                        });
                        break;
                    }
            }
        }

    public:
        bool onActionAttack(const ActionNode &action) override
        {
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
                if(motionPtr->frame < 3){
                    return false;
                }

                m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FixedLocMagic
                {
                    u8"雷电僵尸_雷电",
                    u8"运行",
                    currMotion()->x,
                    currMotion()->y,
                    currMotion()->direction - DIR_BEGIN,
                }));
                return true;
            });
            return true;
        }
};
// --- end clientlightboltzombie.hpp ---

// --- merged from clientminotaurguardian.hpp ---
class ClientMinotaurGuardian: public ClientMonster
{
    public:
        ClientMinotaurGuardian(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"潘夜左护卫") || isMonster(u8"潘夜右护卫"));
        }

    protected:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int) const override
        {
            switch(motion){
                case MOTION_MON_STAND  : return {.count =  4};
                case MOTION_MON_WALK   : return {.count =  6};
                case MOTION_MON_ATTACK0: return {.count =  6};
                case MOTION_MON_HITTED : return {.count =  2};
                case MOTION_MON_DIE    : return {.count = 10};
                case MOTION_MON_ATTACK1: return {.count =  6};
                default                : return {};
            }
        }

    protected:
        bool onActionAttack(const ActionNode &) override;
};
// --- end clientminotaurguardian.hpp ---

// --- merged from clientmonkzombie.hpp ---
class ClientMonkZombie: public ClientMonster
{
    public:
        ClientMonkZombie(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc)
        {
            fflassert(isMonster(u8"僧侣僵尸"));
            switch(action.type){
                case ACTION_SPAWN:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_SPAWN,
                            .direction = pathf::dirValid(action.direction) ? action.direction : to_d(DIR_UP),
                            .x = action.x,
                            .y = action.y,
                        });

                        m_currMotion->addTrigger(true, [proc](MotionNode *motionPtr)
                        {
                            if(motionPtr->frame < 9){
                                return false;
                            }

                            proc->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new MonkZombieSpawnEffect_RUN
                            {
                                motionPtr->x,
                                motionPtr->y,
                                motionPtr->direction - DIR_BEGIN,
                            }));
                            return true;
                        });
                        break;
                    }
                case ACTION_DIE:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_DIE,
                            .direction = pathf::dirValid(action.direction) ? action.direction : to_d(DIR_UP),
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
};
// --- end clientmonkzombie.hpp ---

// --- merged from clientnumawizard.hpp ---
class ClientNumaWizard: public ClientMonster
{
    public:
        ClientNumaWizard(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"诺玛法老") || isMonster(u8"诺玛大法老"));
        }

    public:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int direction) const override
        {
            if(isMonster(u8"诺玛大法老") && (motion == MOTION_MON_ATTACK0)){
                return
                {
                    .gfxMotionID = MOTION_MON_SPELL0,
                    .count = 6,
                };
            }
            return ClientMonster::getFrameGfxSeq(motion, direction);
        }

    protected:
        bool onActionAttack(const ActionNode &action)
        {
            if(isMonster(u8"诺玛法老")){
                return onActionAttack_fireBall(action);
            }
            else if(isMonster(u8"诺玛大法老")){
                return onActionAttack_thunderBolt(action);
            }
            else{
                throw fflpanic("invalid monster: {}", to_cstr(monsterName()));
            }
        }

    private:
        bool onActionAttack_fireBall   (const ActionNode &action);
        bool onActionAttack_thunderBolt(const ActionNode &action);
};
// --- end clientnumawizard.hpp ---

// --- merged from clientrebornzombie.hpp ---
class ClientRebornZombie: public ClientStandMonster
{
    public:
        ClientRebornZombie(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientStandMonster(uid, proc)
        {
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

                        m_standMode = to_bool(action.extParam.stand.sandGhost.standMode);
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
                case ACTION_TRANSF:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_SPAWN,
                            .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                            .x = action.x,
                            .y = action.y,
                        });

                        m_standMode = to_bool(action.extParam.transf.sandGhost.standModeReq);
                        break;
                    }
                case ACTION_MOVE:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_SPAWN,
                            .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                            .x = action.x,
                            .y = action.y,
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

    public:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int) const override
        {
            if(m_standMode){
                switch(motion){
                    case MOTION_MON_SPAWN : return {.count = 10};
                    case MOTION_MON_STAND  : return {.count =  4};
                    case MOTION_MON_WALK   : return {.count =  6};
                    case MOTION_MON_ATTACK0: return {.count =  6};
                    case MOTION_MON_HITTED : return {.count =  2};
                    case MOTION_MON_DIE    : return {.count = 10};
                    default                : return {};
                }
            }
            else{
                switch(motion){
                    case MOTION_MON_STAND: return {.gfxMotionID = MOTION_MON_DIE, .begin = 9, .count =  1};
                    case MOTION_MON_SPAWN: return {.gfxMotionID = MOTION_MON_DIE, .begin = 0, .count = 10};
                    default              : return {};
                }
            }
        }

    protected:
        bool onActionSpawn (const ActionNode &) override;
        bool onActionStand (const ActionNode &) override;
        bool onActionTransf(const ActionNode &) override;
        bool onActionAttack(const ActionNode &) override;

    public:
        bool canFocus(int pointX, int pointY) const override
        {
            return ClientCreature::canFocus(pointX, pointY) && m_standMode;
        }
};
// --- end clientrebornzombie.hpp ---

// --- merged from clientredclothwizard.hpp ---
class ClientRedClothWizard: public ClientMonster
{
    public:
        ClientRedClothWizard(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"红衣法师"));
        }

    public:
        bool onActionAttack(const ActionNode &action) override
        {
            const auto [endX, endY, endDir] = motionEndGLoc().at(1);
            m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
            m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_ATTACK0,
                .direction = m_processRun->getAimDirection(action, endDir),
                .x = action.x,
                .y = action.y,
            }));

            m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, proc = m_processRun](MotionNode *motionPtr) -> bool
            {
                if(motionPtr->frame < 3){
                    return false;
                }

                if(auto coPtr = proc->findUID(targetUID)){
                    coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(u8"红衣法师_魔法", u8"运行")));
                }
                return true;
            });
            return true;
        }
};
// --- end clientredclothwizard.hpp ---

// --- merged from clientsandcactus.hpp ---
class ClientSandCactus: public ClientMonster
{
    public:
        ClientSandCactus(uint64_t, ProcessRun *, const ActionNode &);

    protected:
        bool onActionAttack(const ActionNode &) override;

    protected:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int) const
        {
            switch(motion){
                case MOTION_MON_STAND  : return {.count =  1};
                case MOTION_MON_ATTACK0: return {.count = 10};
                case MOTION_MON_HITTED : return {.count =  2};
                case MOTION_MON_DIE    : return {.count = 10};
                case MOTION_MON_ATTACK1: return {.count =  6};
                case MOTION_MON_SPELL0 : return {.count = 10};
                default                : return {};
            }
        }
};
// --- end clientsandcactus.hpp ---

// --- merged from clientsandevilfan.hpp ---
class ClientSandEvilFan: public ClientMonster
{
    public:
        ClientSandEvilFan(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"沙漠风魔"));
        }

    public:
        bool onActionAttack(const ActionNode &action) override
        {
            const auto [endX, endY, endDir] = motionEndGLoc().at(1);
            m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
            m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_ATTACK0,
                .direction = m_processRun->getAimDirection(action, endDir),
                .x = action.x,
                .y = action.y,
            }));

            m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, proc = m_processRun](MotionNode *motionPtr) -> bool
            {
                if(motionPtr->frame < 3){
                    return false;
                }

                if(auto coPtr = proc->findUID(targetUID)){
                    coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(u8"沙漠风魔_扇风", u8"运行")));
                }
                return true;
            });
            return true;
        }
};
// --- end clientsandevilfan.hpp ---

// --- merged from clientsandghost.hpp ---
class ClientSandGhost: public ClientStandMonster
{
    public:
        ClientSandGhost(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientStandMonster(uid, proc)
        {
            fflassert(isMonster(u8"沙鬼"));
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

                        m_standMode = to_bool(action.extParam.stand.sandGhost.standMode);
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
                case ACTION_TRANSF:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_SPAWN,
                            .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                            .x = action.x,
                            .y = action.y,
                        });

                        m_standMode = to_bool(action.extParam.transf.sandGhost.standModeReq);
                        break;
                    }
                case ACTION_MOVE:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_SPAWN,
                            .direction = pathf::dirValid(action.direction) ? to_d(action.direction) : DIR_UP,
                            .x = action.x,
                            .y = action.y,
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

    public:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int) const override
        {
            if(m_standMode){
                switch(motion){
                    case MOTION_MON_SPAWN:
                        {
                            return
                            {
                                .begin = 9,
                                .count = 10,
                                .reverse = true,
                            };
                        }
                    case MOTION_MON_STAND  : return {.count =  4};
                    case MOTION_MON_WALK   : return {.count =  6};
                    case MOTION_MON_ATTACK0: return {.count =  6};
                    case MOTION_MON_HITTED : return {.count =  2};
                    case MOTION_MON_DIE    : return {.count = 10};
                    default                : return {};
                }
            }
            else{
                switch(motion){
                    case MOTION_MON_STAND: return {.gfxMotionID = MOTION_MON_SPAWN, .begin = 9, .count =  1};
                    case MOTION_MON_SPAWN: return {.gfxMotionID = MOTION_MON_SPAWN, .begin = 0, .count = 10};
                    default               : return {};
                }
            }
        }

    protected:
        bool onActionSpawn (const ActionNode &) override;
        bool onActionStand (const ActionNode &) override;
        bool onActionTransf(const ActionNode &) override;
        bool onActionAttack(const ActionNode &) override;

    public:
        bool canFocus(int pointX, int pointY) const override
        {
            return ClientCreature::canFocus(pointX, pointY) && m_standMode;
        }
};
// --- end clientsandghost.hpp ---

// --- merged from clientsandstoneman.hpp ---
class ClientSandStoneMan: public ClientMonster
{
    public:
        ClientSandStoneMan(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc)
        {
            fflassert(isMonster(u8"沙漠石人"));
            switch(action.type){
                case ACTION_SPAWN:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_SPAWN,
                            .direction = pathf::dirValid(action.direction) ? action.direction : to_d(DIR_UP),
                            .x = action.x,
                            .y = action.y,
                        });

                        m_currMotion->addTrigger(true, [proc](MotionNode *motionPtr)
                        {
                            if(motionPtr->frame < 9){
                                return false;
                            }

                            proc->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new StoneManSpawnEffect_RUN
                            {
                                motionPtr->x,
                                motionPtr->y,
                                motionPtr->direction - DIR_BEGIN,
                            }));
                            return true;
                        });
                        break;
                    }
                case ACTION_DIE:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_DIE,
                            .direction = pathf::dirValid(action.direction) ? action.direction : to_d(DIR_UP),
                            .x = action.x,
                            .y = action.y,
                        });

                        m_currMotion->addTrigger(true, [proc](MotionNode *motionPtr)
                        {
                            if(motionPtr->frame < 4){
                                return false;
                            }

                            proc->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FixedLocMagic
                            {
                                u8"沙漠石人_死亡",
                                u8"运行",
                                motionPtr->x,
                                motionPtr->y,
                            }));
                            return true;
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
};
// --- end clientsandstoneman.hpp ---

// --- merged from clientshipwrecklord.hpp ---
class ClientShipwreckLord: public ClientMonster
{
    public:
        ClientShipwreckLord(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"霸王教主"));
        }

    protected:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int) const override
        {
            switch(motion){
                case MOTION_MON_STAND  : return {.count =  4};
                case MOTION_MON_WALK   : return {.count =  6};
                case MOTION_MON_ATTACK0: return {.count = 10};
                case MOTION_MON_HITTED : return {.count =  2};
                case MOTION_MON_DIE    : return {.count = 10};
                case MOTION_MON_SPELL0 : return {.count = 10};
                default                : return {};
            }
        }

    protected:
        bool onActionAttack(const ActionNode &) override;
};
// --- end clientshipwrecklord.hpp ---

// --- merged from clienttaodog.hpp ---
class ClientTaoDog: public ClientStandMonster
{
    public:
        ClientTaoDog(uint64_t, ProcessRun *, const ActionNode &);

    public:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int) const override
        {
            if(m_standMode){
                switch(motion){
                    case MOTION_MON_STAND  : return {.count =  4};
                    case MOTION_MON_WALK   : return {.count =  6};
                    case MOTION_MON_HITTED : return {.count =  2};
                    case MOTION_MON_DIE    : return {.count = 10};
                    case MOTION_MON_ATTACK0: return {.count =  6};
                    case MOTION_MON_SPAWN  : return {.count = 10}; // from crawling to stand
                    default                : return {};
                }
            }
            else{
                switch(motion){
                    case MOTION_MON_STAND  : return {.gfxLookID = 0X59, .count =  4};
                    case MOTION_MON_WALK   : return {.gfxLookID = 0X59, .count =  6};
                    case MOTION_MON_HITTED : return {.gfxLookID = 0X59, .count =  2};
                    case MOTION_MON_DIE    : return {.gfxLookID = 0X59, .count = 10};
                    case MOTION_MON_SPECIAL:
                        {
                            // from non to crawling
                            // it's not used for standMode transf
                            return
                            {
                                .gfxLookID = 0X59,
                                .gfxMotionID = MOTION_MON_SPAWN,
                                .count = 10,
                            };
                        }
                    case MOTION_MON_SPAWN:
                        {
                            // from stand to crawling
                            // need gfxID redirect and lookID redirect
                            return
                            {
                                .begin = 9,
                                .count = 10,
                                .reverse = true,
                            };
                        }
                    default:
                        {
                            return {};
                        }
                }
            }
        }

    protected:
        bool onActionStand (const ActionNode &) override;
        bool onActionSpawn (const ActionNode &) override;
        bool onActionTransf(const ActionNode &) override;
        bool onActionAttack(const ActionNode &) override;
};
// --- end clienttaodog.hpp ---

// --- merged from clienttaoskeleton.hpp ---
class ClientTaoSkeleton: public ClientMonster
{
    public:
        ClientTaoSkeleton(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc)
        {
            fflassert(isMonster(u8"变异骷髅"));
            m_currMotion.reset(new MotionNode
            {
                .type = MOTION_MON_STAND,
                .direction = [&action]() -> int
                {
                    if(action.type == ACTION_SPAWN){
                        return DIR_DOWNLEFT;
                    }
                    else if(pathf::dirValid(action.direction)){
                        return action.direction;
                    }
                    else{
                        return DIR_UP;
                    }
                }(),

                .x = action.x,
                .y = action.y,
            });
        }
};
// --- end clienttaoskeleton.hpp ---

// --- merged from clienttaoskeletonext.hpp ---
class ClientTaoSkeletonExt: public ClientMonster
{
    public:
        ClientTaoSkeletonExt(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc)
        {
            fflassert(isMonster(u8"超强骷髅"));
            m_currMotion.reset(new MotionNode
            {
                .type = MOTION_MON_STAND,
                .direction = [&action]() -> int
                {
                    if(action.type == ACTION_SPAWN){
                        return DIR_DOWNLEFT;
                    }
                    else if(pathf::dirValid(action.direction)){
                        return action.direction;
                    }
                    else{
                        return DIR_UP;
                    }
                }(),

                .x = action.x,
                .y = action.y,
            });
        }
};
// --- end clienttaoskeletonext.hpp ---

// --- merged from clienttree.hpp ---
class ClientTree: public ClientMonster
{
    public:
        ClientTree(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc)
        {
            switch(action.type){
                case ACTION_HITTED:
                    {
                        m_currMotion.reset(new MotionNode
                        {
                            .type = MOTION_MON_HITTED,
                            .direction = DIR_BEGIN,
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
                            .direction = DIR_BEGIN,
                            .x = action.x,
                            .y = action.y,
                        });
                        break;
                    }
            }
        }

    public:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int) const override
        {
            switch(motion){
                case MOTION_MON_HITTED:
                    {
                        return
                        {
                            .gfxMotionID = MOTION_MON_HITTED,
                            .gfxDirectionID = DIR_BEGIN,
                            .count = 2,
                        };
                    }
                default:
                    {
                        return
                        {
                            .gfxMotionID = MOTION_MON_STAND,
                            .gfxDirectionID = DIR_BEGIN,
                            .count = 4,
                        };
                    }
            }
        }
};
// --- end clienttree.hpp ---

// --- merged from clientwedgemoth.hpp ---
class ClientWedgeMoth: public ClientMonster
{
    public:
        ClientWedgeMoth(uint64_t, ProcessRun *, const ActionNode &);

    protected:
        bool onActionAttack(const ActionNode &) override;
};
// --- end clientwedgemoth.hpp ---

// --- merged from clientwoomaflamingwarrior.hpp ---
class ClientWoomaFlamingWarrior: public ClientMonster
{
    public:
        ClientWoomaFlamingWarrior(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"火焰沃玛"));
        }

    protected:
        bool onActionAttack(const ActionNode &action)
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
                if(motionPtr->frame < 3){
                    return false;
                }

                m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FixedLocMagic
                {
                    u8"火焰沃玛_喷火",
                    u8"运行",
                    currMotion()->x,
                    currMotion()->y,
                    currMotion()->direction - DIR_BEGIN,
                }));
                return true;
            });
            return true;
        }
};
// --- end clientwoomaflamingwarrior.hpp ---

// --- merged from clientwoomataurus.hpp ---
class ClientWoomaTaurus: public ClientMonster
{
    public:
        ClientWoomaTaurus(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"沃玛教主"));
        }

    protected:
        bool onActionAttack(const ActionNode &action)
        {
            m_motionQueue.push_back(std::unique_ptr<MotionNode>(new MotionNode
            {
                .type = MOTION_MON_ATTACK0,
                .direction = m_processRun->getAimDirection(action, currMotion()->direction),
                .x = action.x,
                .y = action.y,
            }));

            switch(action.extParam.attack.magicID){
                case DBCOM_MAGICID(u8"沃玛教主_电光"):
                    {
                        m_motionQueue.back()->addTrigger(false, [this](MotionNode *motionPtr) -> bool
                        {
                            if(motionPtr->frame < 1){
                                return false;
                            }

                            m_processRun->addFixedLocMagic(std::unique_ptr<FixedLocMagic>(new FixedLocMagic
                            {
                                u8"沃玛教主_电光",
                                u8"运行",
                                currMotion()->x,
                                currMotion()->y,
                                currMotion()->direction - DIR_BEGIN,
                            }));
                            return true;
                        });
                        return true;
                    }
                case DBCOM_MAGICID(u8"沃玛教主_雷电术"):
                    {
                        m_motionQueue.back()->addTrigger(false, [targetUID = action.aimUID, this](MotionNode *motionPtr) -> bool
                        {
                            if(motionPtr->frame < 3){
                                return false;
                            }

                            if(auto coPtr = m_processRun->findUID(targetUID)){
                                coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new Thunderbolt()));
                            }
                            return true;
                        });
                        return true;
                    }
                default:
                    {
                        throw fflpanic("invalid DC: id = {}, name = {}", action.extParam.attack.magicID, to_cstr(DBCOM_MAGICRECORD(action.extParam.attack.magicID).name));
                    }
            }
        }
};
// --- end clientwoomataurus.hpp ---

// --- merged from clientzumaarcher.hpp ---
class ClientZumaArcher: public ClientMonster
{
    public:
        ClientZumaArcher(uint64_t uid, ProcessRun *proc, const ActionNode &action)
            : ClientMonster(uid, proc, action)
        {
            fflassert(isMonster(u8"祖玛弓箭手"));
        }

    protected:
        bool onActionAttack(const ActionNode &action) override
        {
            const auto [endX, endY, endDir] = motionEndGLoc().at(1);
            m_motionQueue = makeWalkMotionQueue(endX, endY, action.x, action.y, SYS_MAXSPEED);
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

                const auto gfx16DirIndex = [targetUID, motionPtr, this]() -> int
                {
                    if(auto coPtr = m_processRun->findUID(targetUID)){
                        return pathf::getDir16((coPtr->x() - motionPtr->x) * SYS_MAPGRIDXP, (coPtr->y() - motionPtr->y) * SYS_MAPGRIDYP);
                    }
                    return (motionPtr->direction - DIR_BEGIN) * 2;
                }();

                m_processRun->addFollowUIDMagic(std::unique_ptr<FollowUIDMagic>(new FollowUIDMagic
                {
                    u8"祖玛弓箭手_射箭",
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
                        coPtr->addAttachMagic(std::unique_ptr<AttachMagic>(new AttachMagic(u8"祖玛弓箭手_射箭", u8"裂解")));
                    }
                });
                return true;
            });
            return true;
        }
};
// --- end clientzumaarcher.hpp ---

// --- merged from clientzumamonster.hpp ---
class ClientZumaMonster: public ClientStandMonster
{
    public:
        ClientZumaMonster(uint64_t, ProcessRun *, const ActionNode &);

    public:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int dir) const override
        {
            if(m_standMode){
                switch(motion){
                    case MOTION_MON_SPAWN:
                        {
                            return
                            {
                                .gfxMotionID = MOTION_MON_SPAWN,
                                .begin = 0,
                                .count = 6,
                            };
                        }
                    default:
                        {
                            return ClientStandMonster::getFrameGfxSeq(motion, dir);
                        }
                }
            }
            else{
                switch(motion){
                    case MOTION_MON_STAND:
                        {
                            return
                            {
                                .gfxMotionID = MOTION_MON_SPAWN,
                                .begin = 0,
                                .count = 1,
                            };
                        }
                    case MOTION_MON_SPAWN:
                        {
                            return
                            {
                                .gfxMotionID = MOTION_MON_SPAWN,
                                .begin   = 5,
                                .count   = 6,
                                .reverse = true,
                            };
                        }
                    default:
                        {
                            return {};
                        }
                }
            }
        }

    protected:
        bool onActionSpawn (const ActionNode &) override;
        bool onActionStand (const ActionNode &) override;
        bool onActionTransf(const ActionNode &) override;
        bool onActionAttack(const ActionNode &) override;

    public:
        bool canFocus(int pointX, int pointY) const override
        {
            return ClientCreature::canFocus(pointX, pointY) && m_standMode;
        }
};
// --- end clientzumamonster.hpp ---

// --- merged from clientzumataurus.hpp ---
class ClientZumaTaurus: public ClientStandMonster
{
    public:
        ClientZumaTaurus(uint64_t, ProcessRun *, const ActionNode &);

    public:
        MonsterFrameGfxSeq getFrameGfxSeq(int motion, int dir) const override
        {
            if(m_standMode){
                return ClientStandMonster::getFrameGfxSeq(motion, dir);
            }
            else{
                switch(motion){
                    case MOTION_MON_STAND:
                        {
                            return
                            {
                                .gfxMotionID = MOTION_MON_SPAWN,
                                .begin = 0,
                                .count = 1,
                            };
                        }
                    default:
                        {
                            return {};
                        }
                }
            }
        }

    protected:
        bool onActionSpawn (const ActionNode &) override;
        bool onActionStand (const ActionNode &) override;
        bool onActionTransf(const ActionNode &) override;
        bool onActionAttack(const ActionNode &) override;

    public:
        bool canFocus(int pointX, int pointY) const override
        {
            return ClientCreature::canFocus(pointX, pointY) && m_standMode;
        }

    protected:
        void addActionTransf() override;
};
// --- end clientzumataurus.hpp ---
