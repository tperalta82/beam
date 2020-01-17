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

#pragma once

#include "wallet/client/wallet_client.h"

namespace beam::wallet
{
    using WalletSubscriber = ScopedSubscriber<wallet::IWalletObserver, wallet::Wallet>;
    using WalletDbSubscriber = ScopedSubscriber<wallet::IWalletDbObserver, wallet::IWalletDB>;
    using NodeNetworkSubscriber = ScopedSubscriber<wallet::INodeConnectionObserver, wallet::NodeNetwork>;
#ifdef BEAM_ATOMIC_SWAP_SUPPORT
    using SwapOffersBoardSubscriber = ScopedSubscriber<wallet::ISwapOffersObserver, wallet::SwapOffersBoard>;
#endif

    struct WalletEnvironment
    {

        WalletEnvironment(io::Reactor::Ptr reactor,
                          IWalletDB::Ptr walletStorage,
                          IPrivateKeyKeeper::Ptr keyKeeper,
                          const std::string& nodeAddress)
            : m_reactor(reactor)
            , m_walletDB(walletStorage)
            , m_keyKeeper(keyKeeper)
            , m_nodeAddress(nodeAddress)
        {};

        // external defined
        io::Reactor::Ptr m_reactor;
        IWalletDB::Ptr m_walletDB;
        IPrivateKeyKeeper::Ptr m_keyKeeper;
        const std::string m_nodeAddress;
        // created with WalletCreator
        std::shared_ptr<Wallet> m_wallet;
        std::shared_ptr<NodeNetwork> m_nodeNetwork;
        std::shared_ptr<IWalletMessageEndpoint> m_walletNetwork;
        std::shared_ptr<WalletClient> m_walletClient;
        // scoped subscribers
        std::unique_ptr<WalletSubscriber> m_walletObserver;
        std::unique_ptr<NodeNetworkSubscriber> m_nodeNetworkObserver;
        // TODO move from here
#ifdef BEAM_ATOMIC_SWAP_SUPPORT
        std::shared_ptr<OfferBoardProtocolHandler> m_protocolHandler;
        std::shared_ptr<SwapOffersBoard> m_offersBulletinBoard;
        std::unique_ptr<WalletDbSubscriber> m_walletDbSubscriber;
        std::unique_ptr<SwapOffersBoardSubscriber> m_swapOffersBoardSubscriber;
#endif
    };
} // namespace beam::wallet