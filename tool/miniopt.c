/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

/**
 * @brief miniopt library implementation.
 */

#include "miniopt.h"

//#define DEBUG
#ifdef DEBUG
#include <stdio.h>
#define dbg(x) printf(x);
#else
#define dbg(x) 
#endif

/**
 * @brief option_context state.
 */
enum option_context_state {
    state_start = 0,            ///< Working start.
    state_finished,             ///< Working finished.
    state_double_dash,          ///< Run into no option argument state.
    state_short_opt_no_arg,     ///< Run into short option has no arg state.
    state_error                 ///< Error state.
};

/**
 * @brief option_context.
 */
typedef struct option_context_ {
    int argc;               ///< Command line arg size.
    char **argv;            ///< Command line args.

    option *opts;           ///< Option array.
    int optsum;             ///< Option array size.

    int optind;             ///< Current option index;
    const char *optarg;     ///< Current option-arg or non-option-arg.
    const char *error;      ///< Current error message;

    int index;              ///< Current parsing index to argv.
    const char *token;      ///< Current parsing token.
    const char *it;         ///< Forward iterator to the parsing token.
    int state;              ///< Current parsing state.
} option_context;

static option_context optctx;   // Global option context.

/**
 * @brief Convert number to decimal number string.
 * 
 * @param[in] number        A number to convert.
 * @return const char*      Decimal number string.
 */
static const char *miniopt_to_string(int number) {
    static char buf[32];
    static const int bufsize = sizeof(buf) / sizeof(buf[0]);
    static const char dict[] = "0123456789";

    buf[bufsize - 1] = '\0';
    int i = bufsize - 2;
    int minus = 0;
    int temp;

    if (number < 0) {
        minus = 1;
        number *= -1;
    }

    do {
        temp = number;
        number /= 10;
        buf[i--] = dict[temp - number * 10];
    } while (number);

    minus ? (buf[i] = '-') : (++i);

    int length = bufsize - i;
    for (int j = 0; j < length; ++j) { buf[j] = buf[i++]; }

    return buf;
}

/**
 * @brief Concat string.
 *
 * @param[in] buf       Destination buf.
 * @param[in] bufsz     Destination buf size.
 * @param[in] s1        Source string1.
 * @param[in] s2        Source string2.
 * @param[in] s3        Source string3.
 *
 * @return int >= 0     Concat string pass.
 * @return -1           The buffer cannot hold all the string(s).
 */
static int miniopt_concat(char *buf, int bufsz, const char *s1, const char *s2,
                   const char *s3) {
    const char *strArray[] = {s1, s2, s3};
    const int strArraySize = sizeof(strArray) / sizeof(strArray[0]);

    int i = 0;
    for (int j = 0; j < strArraySize; ++j) {
        const char *s = strArray[j];
        if (s == nil) continue;
        if (i >= bufsz) break;
        while (i < bufsz && *s) { buf[i++] = *s++; }
    }

    if (i < bufsz) {
        buf[i] = '\0';
        return i + 1;
    } else {
        miniopt_assert(0);      // Cannot goto here.
        buf[bufsz - 1] = '\0';  // Force string don't overflow.
        return -1;
    }
}

/**
 * @brief Make an error string.
 * 
 * It will change parsing state to error state.
 * 
 * @param[in] s1    Input string1
 * @param[in] s2    Input string2
 * @param[in] s3    Input string3
 */
static void miniopt_make_error(const char *s1, const char *s2, const char *s3) {
    static char error[ERROR_STR_MAX_SIZE];

    miniopt_assert(!(s1 == nil && s2 == nil && s3 == nil));

    if (s1 == nil && s2 == nil && s3 == nil) {
        error[0] = '\0';
    } else {
        miniopt_concat(error, ERROR_STR_MAX_SIZE, s1, s2, s3);
    }

    optctx.error = error;
    optctx.state = state_error;
}

/**
 * @brief Peek next token.
 * 
 * @return NOT_nil     Next token.
 * @return nil         No more token exist. 
 */
static const char *miniopt_peek_next_token() {
    miniopt_assert(optctx.argc > 0);
    miniopt_assert(optctx.argv != nil);

    if (optctx.argc <= 1) { // Only one arg is the file path, skip it.
        return nil;
    } else if (optctx.index == -1) { // It is first time to peek token.
        return optctx.argv[1];
    } else if ((optctx.index + 1) < optctx.argc) { // Not first time.
        return optctx.argv[optctx.index + 1];
    } else {
        return nil; // No more tokens.
    }
}

/**
 * @brief Get next token.
 * 
 * @return NOT_nil     Next token.
 * @return nil         No more token to get. 
 */
static const char *miniopt_get_next_token() {
    miniopt_assert(optctx.argc > 0);
    miniopt_assert(optctx.argv != nil);

    if (optctx.argc <= 1) { // Only one arg is the file path, skip it.
        return nil;
    } else if (optctx.index == -1) { // It is first time to get token.
        optctx.index = 1;
        return optctx.argv[optctx.index];
    } else if (++optctx.index < optctx.argc) { // Not first time.
        return optctx.argv[optctx.index];
    } else {
        return nil; // No more tokens.
    }
}

/**
 * @brief Check whether an option has or requires an argument.
 * 
 * @param[in] opt   Input option to check.
 * 
 * @return 0        It has no argument.
 * @return 1        It has an argument.
 */
static int miniopt_opt_has_arg(option* opt)
{
    miniopt_assert(opt != nil);

    return opt->ahint != nil;
}

/**
 * @brief Check whether input char is short option.
 * 
 * @param[in] c         Input char.
 * @param[out] hasArg   Whether this option need argument.
 * @param[out] optind   Option index to the option array.
 * 
 * @return 1            It is short option.
 * @return 0            It is not short option.
 */
static int miniopt_is_short_option(char c, int *hasArg, int *optind) {
    miniopt_assert(hasArg != nil);
    miniopt_assert(optind != nil);

    if (c == '-' || c == '=' || c == 0) return 0;

    *hasArg = 0;
    *optind = optctx.optsum;
    for (int i = 0; i < optctx.optsum; ++i) {
        if (optctx.opts[i].sname == c) {
            *hasArg = miniopt_opt_has_arg(&(optctx.opts[i]));
            *optind = i;
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Compare two string.
 * 
 * @param[in] beg   First string begin.
 * @param[in] end   First string end.
 * @param[in] str   Second string.
 * 
 * @return 1        They are the same.
 * @return 0        They are not the same.
 */
static int miniopt_is_same(const char *beg, const char *end, const char *str) {
    miniopt_assert(beg != nil);
    miniopt_assert(end != nil);
    miniopt_assert(str != nil);

    while (beg < end && *beg == *str) {
        ++beg;
        ++str;
    }
    
    return (beg == end && *str == '\0');
}

/**
 * @brief Check whether input string is long option.
 * 
 * @param[in] beg       Input string begin.
 * @param[in] end       Input string end.
 * @param[out] hasArg   Whether this option requires an argument.
 * @param[out] optind   Option index to the option array.
 * 
 * @return 1            It is long option.
 * @return 0            It is not long option.
 */
static int miniopt_is_long_option(const char *beg, const char *end, int *hasArg,
                           int *optind) {
    miniopt_assert(hasArg != nil);
    miniopt_assert(optind != nil);

    *hasArg = 0;
    *optind = optctx.optsum;

    if(beg == nil || end == nil) return 0;

    if (beg < end) {
        for (int i = 0; i < optctx.optsum; ++i) {
            if (miniopt_is_same(beg, end, optctx.opts[i].lname)) {
                *hasArg = miniopt_opt_has_arg(&(optctx.opts[i]));
                *optind = i;
                return 1;
            }
        }
    }
    return 0;
}

static int miniopt_strlen(const char* s)
{
    int len = 0;
    if(s){
        while(*s){
            ++s;
            ++len;
        }
    }
    return len;
}

/**
 * @brief Find char in string.
 * 
 * @param[in] str   Input string.
 * @param[in] c     Input char, it should not be string terminator '\0'.
 * 
 * @return 1        Find ok.
 * @return 0        Find fail.
 */
static int miniopt_find(const char *str, char c) {
    if (str) {
        while (*str) {
            if (*str++ == c) return 1;
        }
    }
    return 0;
}

/**
 * @brief Simple check input options.
 * 
 * @return 0    Check pass.
 * @return -1   Check fail. 
 */
static int miniopt_simple_check() {
    dbg("check options begin...\n");

    for (int i = 0; i < optctx.optsum; ++i) {
        if (optctx.opts[i].sname == nil && optctx.opts[i].lname == nil) {
            miniopt_make_error(
                "Option index = ", miniopt_to_string(i),
                ", At least one of the short and long names cannot be nil.");
            return -1;
        }
        if (optctx.opts[i].sname == '-') {
            miniopt_make_error(
                "Option index = ", miniopt_to_string(i),
                ", Character [-] cannot be used as short option.");
            return -1;
        }
        if (optctx.opts[i].sname == '=') {
            miniopt_make_error(
                "Option index = ", miniopt_to_string(i),
                ", Character [=] cannot be used as short option.");
            return -1;
        }
        if (optctx.opts[i].lname && optctx.opts[i].lname[0] == '-') {
            miniopt_make_error(
                "Option index = ", miniopt_to_string(i),
                ", Character [-] cannot be long option's first char.");
            return -1;
        }
        if (miniopt_find(optctx.opts[i].lname, '=')) {
            miniopt_make_error(
                "Option index = ", miniopt_to_string(i),
                ", Character [=] cannot be used in long option.");
            return -1;
        }
        if(miniopt_strlen(optctx.opts[i].lname) > OPTION_NAME_MAX_SIZE){
            miniopt_make_error(
                "Option index = ", miniopt_to_string(i),
                ", long name size cannot more than OPTION_NAME_MAX_SIZE.");
            return -1; 
        }
    }

    dbg("check options pass.\n");

    return 0;
}

/**
 * @brief Initialize miniopt object.
 *
 * @param[in] argc      Argument array size.
 * @param[in] argv      Argument array.
 * @param[in] opts      Option array.
 * @param[in] optsum    Option sum of the option array.
 *
 * @return 0            Init pass.
 * @return other        Init error.
 */
static int miniopt_init_impl(int argc, char **argv, option *opts, int optsum) {
    miniopt_assert(argc > 0);
    miniopt_assert(argv != nil);
    miniopt_assert(opts != nil);
    miniopt_assert(optsum > 0);

    optctx.argc = argc;
    optctx.argv = argv;

    optctx.opts = opts;
    optctx.optsum = optsum;

    optctx.optind = optsum;
    optctx.optarg = nil;
    optctx.error = nil;

    optctx.index = -1;
    optctx.token = nil;
    optctx.it = nil;
    optctx.state = state_start;

    return miniopt_simple_check();
}

/**
 * @brief Get next option.
 * 
 * It should be used after miniopt.init();
 *
 * @return MINIOPT_PASS    Get next option pass.
 * @return MINIOPT_FINISHED     Work finished ok(no more option to get).
 * @return MINIOPT_ERROR        Work finished or stopped with error.
 */
static int miniopt_getopt_impl() {
    switch (optctx.state) {
        case state_start: {
            optctx.token = miniopt_get_next_token();
            if (optctx.token == nil) {
                optctx.state = state_finished;
                break;
            } else if (optctx.token[0] == '-') {
                // May be short option.
                int needArg;
                int optind;
                if (miniopt_is_short_option(optctx.token[1], &needArg,
                                            &optind)) {
                    // It is short option like "-x".
                    optctx.optind = optind;
                    if (needArg) {
                        // Short option has an arg.
                        if (optctx.token[2] != 0) {
                            // It is "-xarg" or "-x=arg"
                            if (optctx.token[2] != '=') {
                                // "-xarg"
                                optctx.optarg = &(optctx.token[2]);
                                return MINIOPT_PASS;
                            } else if (optctx.token[2] == '=' 
                                       && optctx.token[3] != 0) {
                                // "-x=arg"
                                optctx.optarg = &(optctx.token[3]);
                                return MINIOPT_PASS;
                            } else {
                                miniopt_make_error(
                                    "option ",
                                    optctx.token,
                                    " argument is missing.");
                                return MINIOPT_ERROR;
                            }
                        } else if (miniopt_peek_next_token() != nil) {
                            // "-x arg"
                            optctx.optarg = miniopt_get_next_token();
                            return MINIOPT_PASS;
                        } else {
                            miniopt_make_error("option ",
                                               optctx.token,
                                               " argument is missing.");
                            return MINIOPT_ERROR;
                        }
                    } else {
                        // (All) short option has no arg.
                        // "-x" equal to "-x"
                        // "-abc" equal to "-a -b -c"
                        optctx.optarg = nil;
                        optctx.it = &(optctx.token[1]);
                        // Run into this state to split "-abc" to "-a -b -c".
                        optctx.state = state_short_opt_no_arg;
                        return MINIOPT_PASS;
                    }
                } else if (optctx.token[1] == '-') { // May be long option.
                    if (optctx.token[2] == '\0') {
                        // Option is "--", it is non-option-argument marker. 
                        optctx.state = state_double_dash;
                        return miniopt_getopt_impl();
                    } else {
                        // Find a long option.
                        const char *beg = &(optctx.token[2]);
                        const char *end = beg;
                        // Long option cannot begin with '=' or '-';
                        if (*beg != '\0' && *beg != '=' && *beg != '-') {
                            ++end;
                            while (*end != '\0' && *end != '=') { ++end; }
                        }

                        int needArg;
                        int optind;
                        if (miniopt_is_long_option(beg, end, &needArg,
                                                   &optind)) {
                            // option like "--key...";
                            optctx.optind = optind;
                            if (needArg) {
                                // option like "--key=value" or "--key value"
                                if (*(end) != '\0') {
                                    // " --key=value"
                                    if (*end == '=' && *(end + 1) != '\0') {
                                        optctx.optarg = end + 1;
                                        return MINIOPT_PASS;
                                    } else {
                                        miniopt_make_error(
                                            "option ",
                                            optctx.token,
                                            " argument is missing.");
                                        return MINIOPT_ERROR;
                                    }
                                } else if (miniopt_peek_next_token() != nil) {
                                    // "--key value"
                                    optctx.optarg = miniopt_get_next_token();
                                    return MINIOPT_PASS;
                                } else {
                                    miniopt_make_error(
                                        "option ", optctx.token,
                                        ", argument is missing.");
                                    return MINIOPT_ERROR;
                                }
                            } else {
                                if (*end == '\0') {
                                // option like "--key" has no arg.
                                    optctx.optarg = nil;
                                    return MINIOPT_PASS;
                                } else {
                                    miniopt_make_error(
                                        "option ", optctx.token,
                                        " is unknown.");
                                    return MINIOPT_ERROR;
                                }
                            }
                        } else {
                            miniopt_make_error("option ",
                                               optctx.token,
                                               " is unknown.");
                            return MINIOPT_ERROR;
                        }
                    }
                } else {
                    // Token begin with '-', but it is not an option.
                    miniopt_make_error("option ", optctx.token,
                                       " is unknown.");
                    return MINIOPT_ERROR;
                }
            } else if (optctx.token[0] == '\0') {
                // Empty string, just skip it.
                return miniopt_getopt_impl(optctx);
            } else {
                // return non-option-argument;
                optctx.optind = optctx.optsum;
                optctx.optarg = optctx.token;
                return MINIOPT_PASS;
            }
        }
        case state_double_dash: {
            // All the tokens are non-option-argument;
            const char *optarg = miniopt_get_next_token();
            if (optarg != nil) {
                optctx.optarg = optarg;
                optctx.optind = optctx.optsum;
                return MINIOPT_PASS;
            } else {
                optctx.state = state_finished;
                break;
            }
        }
        case state_short_opt_no_arg: {
            // split "-abc" to "-a -b -c".
            if (*(++optctx.it) != '\0') {
                int needArg;
                int optind;
                if (miniopt_is_short_option(*(optctx.it), &needArg,
                                            &optind) &&
                    needArg == 0) {
                    optctx.optind = optind;
                    optctx.optarg = nil;
                    return MINIOPT_PASS;
                } else {
                    miniopt_make_error("option ", optctx.token,
                                       " has error.");
                    return MINIOPT_ERROR;
                }
            } else {
                // Restart.
                optctx.state = state_start;
                return miniopt_getopt_impl(optctx);
            }
        }
        case state_error: {
            optctx.optind = optctx.optsum;
            optctx.optarg = nil;
            return MINIOPT_ERROR;
        }
        case state_finished:
            optctx.optind = optctx.optsum;
            optctx.optarg = nil;
        default:
            break;
    }

    return MINIOPT_FINISHED;
}

/**
 * @brief Get current option index to the option array.
 *
 * It should be used after (miniopt.getopt() > 0);
 *
 * Here optsum means sum-of-the-option-array or option-array-size.
 *
 * @return [0, optsum-1]    An option index to the option array.
 * @return optsum           It means miniopt.optarg() is non option argument.
 */
static int miniopt_optind_impl() {
    return optctx.optind;
}

/**
 * @brief Get current option-argument or non-option-argument.
 *
 * It should be used after (miniopt.getopt() > 0);
 *
 * @return not nil     An argument.
 * @return nil         Current option has no argument.
 */
static const char *miniopt_optarg_impl() { 
    return optctx.optarg; 
}

static void miniopt_print_desc(printf_fn printf_, const char* desc, int offset){
    if(!desc) return;
    
    while(*desc){
        if(*desc == '<' 
           && *(desc+1) == 'b' 
           && *(desc+2) == 'r' 
           && *(desc+3) == '>'){
            desc += 4;
            printf_("\n");
            for(int i = 0; i < offset; ++i){
                printf_(" ");
            }
        }
        else{
            printf_("%c", *desc++);
        }
    }
    
    printf_("\n");
}

static void miniopt_print_one_opt(printf_fn printf_,
                           option* opt, 
                           int maxShortOptSize, 
                           int maxLongOptSize, 
                           int maxAhintSize, 
                           int offset,
                           int indention){
    int rest = offset;
    for(int i = 0; i < indention; ++i){
        printf_(" ");
    }
    
    if(opt->sname != nil){
        printf_("-%c", opt->sname);
        rest -= 2;
    }
    else if(maxShortOptSize)
    {
        for(int i = 0; i < maxShortOptSize; ++i){
            printf_(" ");
        }
        rest -= maxShortOptSize;
    }
    
    if(maxShortOptSize && maxLongOptSize){
        printf_(" ");
        rest -= 1;
    }

    if(opt->lname != nil){
        printf_("--%s", opt->lname);
        rest -= miniopt_strlen(opt->lname) + 2;
    }
    
    if(opt->ahint != nil){
        printf_(" %s", opt->ahint);
        rest -= miniopt_strlen(opt->ahint) + 1;
    }
    
    rest -= indention;
    while(rest > 0){
        printf_(" ");
        --rest;
    }

    miniopt_print_desc(printf_, opt->desc, offset);
}


static void miniopt_internal_print_opts(printf_fn printf_, 
                                 option* opts, 
                                 int optsum, 
                                 int indention){
    int maxShortOptSize = 0;
    int maxLongOptSize = 0;
    int maxAhintSize = 0;
    int offset = 0;     // offset to opt.desc.

    for(int i = 0; i < optsum; ++i){
        int total = 0;

        int shortOptSize = opts[i].sname ? 1 : 0;
        if(maxShortOptSize < shortOptSize){
            shortOptSize += 1;  // For the leading '-'
            maxShortOptSize = shortOptSize;
            total += shortOptSize;
        }

        int longOptSize = miniopt_strlen(opts[i].lname);
        if(maxLongOptSize < longOptSize){
            longOptSize += 2;  // For the leading '--'
            maxLongOptSize = longOptSize;
            total += longOptSize;
        }

        int ahintSize = miniopt_strlen(opts[i].ahint);
        if(maxAhintSize < ahintSize){
            ahintSize += 1; // For the leading ' '
            maxAhintSize = ahintSize;
            total += ahintSize;
        }
        if(offset < total){
            offset = total;
        }
    }
    
    offset += indention;
    // Add space between short name and long name like "-k --key".
    if(maxShortOptSize && maxLongOptSize) offset += 1;
    // Add spaces before opt.desc
    offset += 3;
    
    for(int i = 0; i < optsum; ++i){
        miniopt_print_one_opt(printf_,
                              &opts[i], 
                              maxShortOptSize, 
                              maxLongOptSize, 
                              maxAhintSize, 
                              offset,
                              indention);
    }
}

/**
 * @brief Print the option array.
 * 
 * @param[in] indention     Indention at the line beginning.
 */
static void miniopt_printopts_impl(printf_fn printf_, int indention){
    if(printf_ == nil || optctx.opts == nil) return;
    miniopt_internal_print_opts(printf_,
                                optctx.opts, 
                                optctx.optsum, 
                                indention);
}

/**
 * @brief Get current error str.
 *
 * @return not nil     An erro str.
 * @return nil         No error.
 */
static const char *miniopt_what_impl() { 
    return optctx.error; 
}

/**
 * @brief Miniopt singleton object definition.
 */
Miniopt miniopt = {
    miniopt_init_impl,
    miniopt_getopt_impl,
    miniopt_optind_impl,
    miniopt_optarg_impl,
    miniopt_printopts_impl,
    miniopt_what_impl
};
