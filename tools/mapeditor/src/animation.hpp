#pragma once
#include <tuple>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "imguihelper.hpp"
#include "rawbuf.hpp"

class Animation
{
    private:
        struct InnFrame
        {
            int dx = 0;
            int dy = 0;

            // AnimationDB uses initializer_list to initialize its internal vector<Animation>, see AnimationDB::AnimationDB(...)
            // it requests Animation to be copy-constructable
            std::shared_ptr<ImGuiTexture> image {};
        };

    private:
        std::vector<InnFrame> m_frameList;

    public:
        Animation(std::initializer_list<std::tuple<int, int, std::initializer_list<uint8_t>>> ilist)
        {
            for(const auto &[dx, dy, data]: ilist){
                const Rawbuf imgData(data);
                m_frameList.push_back(InnFrame
                {
                    .dx = dx,
                    .dy = dy,
                    .image = std::make_shared<ImGuiTexture>(),
                });
                m_frameList.back().image->loadPNG(imgData.data(), imgData.size());
            }
        }

    public:
        size_t frameCount() const
        {
            return m_frameList.size();
        }

        std::tuple<int, int, ImGuiTexture *> frame(size_t index) const
        {
            return
            {
                m_frameList.at(index).dx,
                m_frameList.at(index).dy,
                m_frameList.at(index).image.get(),
            };
        }
};
