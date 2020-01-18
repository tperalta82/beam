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

#include "wallet/client/wallet_env.h"

namespace beam::wallet
{
    // TODO: try combine WalletCreator with WalletEnvironment
    struct WalletCreator
    {
        /// Create specified WalletClient implementation on WalletEnvironment
        template <class WalletClientType>
        static std::shared_ptr<WalletClientType> createWallet(WalletEnvironment& env, std::shared_ptr<std::unordered_map<TxType, BaseTransaction::Creator::Ptr>> txCreators = std::shared_ptr<std::unordered_map<TxType, BaseTransaction::Creator::Ptr>>())
        {
            io::Reactor::Scope scope(*env.m_reactor);

            env.m_wallet = std::make_shared<Wallet>(env.m_walletDB, env.m_keyKeeper);
            env.m_nodeNetwork = std::make_shared<NodeNetwork>(*env.m_wallet, env.m_nodeAddress);

            // BaseMessageEndpoint .ctor needs io::Reactor::get_Current() for m_AddressExpirationTimer
            env.m_walletNetwork = std::make_shared<WalletNetworkViaBbs>(*env.m_wallet, env.m_nodeNetwork, env.m_walletDB, env.m_keyKeeper);

            env.m_wallet->SetNodeEndpoint(env.m_nodeNetwork);
            env.m_wallet->AddMessageEndpoint(env.m_walletNetwork);
            if (txCreators)
            {
                for (auto& [txType, creator] : *txCreators)
                {
                    env.m_wallet->RegisterTransactionType(txType, creator);
                }
            }

            auto walletClient = std::make_shared<WalletClientType>(env.m_reactor, env.m_walletDB, env.m_keyKeeper, env.m_nodeNetwork, env.m_wallet);
            env.m_walletClient = std::shared_ptr<WalletClient>(walletClient);

            // WalletEnvironment stores shared_ptr<WalletClient> to prevent destruction
            // before it's observers. Thus prevent running callbacks on destroyed WalletClient*.
            env.m_walletObserver = make_unique<WalletSubscriber>(reinterpret_cast<IWalletObserver*>(env.m_walletClient.get()), env.m_wallet);
            env.m_nodeNetworkObserver = make_unique<NodeNetworkSubscriber>(reinterpret_cast<INodeConnectionObserver*>(env.m_walletClient.get()), env.m_nodeNetwork);

#ifdef BEAM_ATOMIC_SWAP_SUPPORT
            env.m_protocolHandler = make_shared<OfferBoardProtocolHandler>(env.m_keyKeeper->get_SbbsKdf(), env.m_walletDB);
            env.m_offersBulletinBoard = make_shared<SwapOffersBoard>(*env.m_nodeNetwork, *env.m_walletNetwork, *env.m_protocolHandler);

            env.m_walletDbSubscriber = make_unique<WalletDbSubscriber>(static_cast<IWalletDbObserver*>(env.m_offersBulletinBoard.get()), env.m_walletDB);
            env.m_swapOffersBoardSubscriber = make_unique<SwapOffersBoardSubscriber>(reinterpret_cast<ISwapOffersObserver*>(env.m_walletClient.get()), env.m_offersBulletinBoard);

            walletClient->attachSwapOfferBoard(env.m_offersBulletinBoard);
#endif

            return walletClient;
        };
    };

} // namespace beam::wallet
