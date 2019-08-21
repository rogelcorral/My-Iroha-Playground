/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef IROHA_PROTO_PROPOSAL_VALIDATOR_HPP
#define IROHA_PROTO_PROPOSAL_VALIDATOR_HPP

#include "proposal.pb.h"
#include "validators/abstract_validator.hpp"

namespace shared_model {
  namespace validation {
    class ProtoProposalValidator
        : public AbstractValidator<iroha::protocol::Proposal> {
     public:
      using ProtoValidatorType =
          std::shared_ptr<shared_model::validation::AbstractValidator<
              typename iroha::protocol::Transaction>>;

      ProtoProposalValidator(ProtoValidatorType transaction_validator);

      Answer validate(const iroha::protocol::Proposal &proposal) const override;

     private:
      ProtoValidatorType transaction_validator_;
    };
  }  // namespace validation
}  // namespace shared_model

#endif  // IROHA_PROTO_PROPOSAL_VALIDATOR_HPP
