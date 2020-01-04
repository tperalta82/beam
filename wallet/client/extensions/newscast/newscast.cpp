// Copyright 2019 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "newscast.h"
#include "utility/logger.h"

namespace beam::wallet
{
    Newscast::Newscast(FlyClient::INetwork& network, NewscastProtocolParser& parser)
        : m_network(network),
          m_parser(parser)
    {
        for (auto channel : m_channels)
        {
            m_network.BbsSubscribe(channel, m_lastTimestamp, this);
        }
    }

    const std::set<BbsChannel> Newscast::m_channels =
    {
        proto::Bbs::s_MaxChannels + BbsChannelsOffset,
    };

    void Newscast::OnMsg(proto::BbsMsg &&msg)
    {
        auto news = m_parser.parseMessage(msg.m_Message);

        if (news.has_value())
        {
            // TODO polymorphic parsing
            notifySubscribers(*news);
        }
    }

    void Newscast::Subscribe(INewsObserver* observer)
    {
        auto it = std::find(m_subscribers.begin(),
                            m_subscribers.end(),
                            observer);
        assert(it == m_subscribers.end());
        m_subscribers.push_back(observer);
    }

    void Newscast::Unsubscribe(INewsObserver* observer)
    {
        auto it = std::find(m_subscribers.begin(),
                            m_subscribers.end(),
                            observer);
        assert(it != m_subscribers.end());
        m_subscribers.erase(it);
    }

    void Newscast::notifySubscribers(NewsMessage msg) const
    {
        for (auto sub : m_subscribers)
        {
            sub->onNewsUpdate(msg);
        }
    }

} // namespace beam::wallet