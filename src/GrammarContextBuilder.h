/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#pragma once

#include "BaseType.h"

namespace csa {

class GrammarContextBuilder {
public:
    static GrammarContextPtr buildFromStream(const std::string &stream);
    static GrammarContextPtr buildFromFile(const std::string &filename);

private:
    static GrammarContextPtr buildFromBuffer(std::vector<char> &buf);
};

}    // namespace csa
