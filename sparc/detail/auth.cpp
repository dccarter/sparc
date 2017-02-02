//
// Created by dc on 1/29/17.
//
#include <openssl/md5.h>
#include <openssl/rand.h>

#include "sparc.h"
#include "auth.h"
#include "kore.h"
#include "http.h"

namespace sparc {
    namespace auth {
        static bool basic(cc_string, c_string *, AuthInfoCb);
        static bool digest(cc_string auth_header,
                           c_string *usr,
                           Method  method,
                           AuthInfoCb getInfo);

        void user_t::destroy() {
            if (username != nullptr) {
                kore_free(username);
                username = nullptr;
            }

            if (password != nullptr) {
                kore_free(password);
                password = nullptr;
            }
        }

        bool basic(cc_string auth_header, c_string *usr, AuthInfoCb getInfo) {
            cc_string base64_auth = strchr(auth_header, ' ');
            bool ok = false;

            do {
                size_t len;
                u_int8_t *out;
                c_string user_pwd[3];
                int ntoks;
                if (!base64_auth) {
                    $debug("request did not provide authorization %s",
                           auth_header ? auth_header : "");
                    break;
                }

                if (!kore_base64_decode(base64_auth + 1, &out, &len)) {
                    $debug("decoding authorization header failed: %s", base64_auth);
                    break;
                }

                ntoks = kore_split_string((char *) out, ":", user_pwd, 3);
                if (ntoks < 2) {
                    $debug("invalid decoded authorization header %s", out);
                    kore_free(out);
                    break;
                }

                *usr = kore_strdup(user_pwd[0]);
                auth_info_t info(AUTH_TYPE_BASIC, user_pwd[0]);
                // the password should be set to the hashed password
                if (!getInfo(info) || !info.password || !info.salt) {
                    kore_debug("auth information for %s not provided by app", *usr);
                    kore_free(out);
                    break;
                }
                // TODO implement hashing
                if (strcmp(user_pwd[1], info.password)) {
                    kore_debug("provided password(%s) does not match stored password %s",
                               user_pwd[1], info.password);
                    kore_free(out);
                    break;
                }
                kore_free(out);
                ok = true;
            }while (0);

            return ok;
        }

#define _dbgstr(st) ((st != NULL)? st : "")

        bool digest(cc_string auth_header,
                    c_string *usr,
                    Method  method,
                    AuthInfoCb getInfo) {

            cc_string digest_start = strchr(auth_header, ' ');
            bool ok = false;
            do {
                uint8_t A1[MD5_DIGEST_LENGTH],
                        A2[MD5_DIGEST_LENGTH],
                        RESPONSE[MD5_DIGEST_LENGTH];
                buffer dbuf(64);
                cc_string dstr;

                if (!digest_start) {
                    kore_debug("invalid authorization header %s", auth_header);
                    break;
                }
                c_string digest_str = kore_strdup(digest_start + 1);
                c_string digest_toks[10];
                int ntoks;
                ntoks = kore_split_string(digest_str, ",", digest_toks, 10);
                if (ntoks < 7) {
                    kore_debug("Authorization header does not contain all fields: %s %d",
                               auth_header, ntoks);
                    kore_free(digest_str);
                    break;
                }
                c_string username = NULL, realm = NULL, nonce = NULL,
                        opaque = NULL, uri = NULL, response = NULL,
                        cnonce = NULL, qop = NULL, algorithm = NULL,
                        nc = NULL;

                int i = 0;
                while (i < ntoks) {
                    kore_debug("handling token(%d)=%s", i, digest_toks[i]);
                    if (!username && strstr(digest_toks[i], "username")) {
                        username = strchr(digest_toks[i], '=') + 2;
                        *strchr(username, '\"') = '\0';
                    } else if (!realm && strstr(digest_toks[i], "realm")) {
                        realm = strchr(digest_toks[i], '=') + 2;
                        *strchr(realm, '\"') = '\0';
                    } else if (!nonce && strstr(digest_toks[i], "nonce")) {
                        nonce = strchr(digest_toks[i], '=') + 2;
                        *strchr(nonce, '\"') = '\0';
                    } else if (!opaque && strstr(digest_toks[i], "opaque")) {
                        opaque = strchr(digest_toks[i], '=') + 2;
                        *strchr(opaque, '\"') = '\0';
                    } else if (!uri && strstr(digest_toks[i], "uri")) {
                        uri = strchr(digest_toks[i], '=') + 2;
                        *strchr(uri, '\"') = '\0';
                    } else if (!response && strstr(digest_toks[i], "response")) {
                        response = strchr(digest_toks[i], '=') + 2;
                        *strchr(response, '\"') = '\0';
                    } else if (!cnonce && strstr(digest_toks[i], "cnonce")) {
                        cnonce = strchr(digest_toks[i], '=') + 2;
                        *strchr(cnonce, '\"') = '\0';
                    } else if (!qop && strstr(digest_toks[i], "qop")) {
                        qop = strchr(digest_toks[i], '=') + 1;
                    } else if (!algorithm && strstr(digest_toks[i], "algorithm")) {
                        algorithm = strchr(digest_toks[i], '=') + 1;
                    } else if (!nc && strstr(digest_toks[i], "nc")) {
                        nc = strchr(digest_toks[i], '=') + 1;
                    }
                    i++;
                }
                kore_debug("digest parameters: "
                                   "username=%s realm=%s nonce=%s opaque=%s "
                                   "uri=%s response=%s cnonce=%s qop=%s nc=%s",
                           _dbgstr(username), _dbgstr(realm),
                           _dbgstr(nonce), _dbgstr(opaque),
                           _dbgstr(uri), _dbgstr(response),
                           _dbgstr(cnonce), _dbgstr(qop),
                           _dbgstr(nc), _dbgstr(algorithm));


                if (username == NULL || realm == NULL || nonce == NULL ||
                    opaque == NULL || uri == NULL || response == NULL) {
                    kore_debug("missing a required field in Authorization header");
                    kore_free(digest_str);
                    break;
                }
                *usr = kore_strdup(username);

                auth_info_t info(AUTH_TYPE_DIGEST, username);
                getInfo(info);
                if (info.password == NULL) {
                    kore_debug("validator did not provide passord for user");
                    kore_free(digest_str);
                    break;
                }
                // A1=md5("username:realm:password")
                dbuf.appendf("%s:%s:%s", username, realm, info.password);
                dstr = dbuf.toString();
                kore_debug(".A1(%d)=%s", dbuf.offset()-1, dstr);
                MD5((uint8_t *)dstr, dbuf.offset()-1, (uint8_t*)&A1);
                dbuf.reset();
                // A2=md5("method:uri")
                dbuf.appendf("%s:%s", http_method_text(method), uri);
                dstr = dbuf.toString();
                kore_debug("A2(%d)=%s", dbuf.offset()-1, dstr);
                MD5((uint8_t *)dstr, dbuf.offset()-1, (uint8_t*)&A2);
                dbuf.reset();
                dbuf.bytes(A1, sizeof(A1));
                if (qop && !strcmp(qop, "auth")) {
                    kore_debug("RESPONSE=md5(\"A1:nounce:nc:cnonce:qop:A2\")");
                    dbuf.appendf(":%s:%s:%s:%s:", _dbgstr(nonce), _dbgstr(nc),
                                 _dbgstr(cnonce), _dbgstr(qop));
                } else {
                    kore_debug("RESPONSE=md5(\"A1:nonce:A2\")");
                    dbuf.appendf(":%s:", nonce);
                }
                dbuf.bytes(A2, sizeof(A2));
                dstr = dbuf.toString();
                kore_debug(".RESP(%d)=%s", dbuf.offset()-1, dstr);
                MD5((uint8_t *)dstr, dbuf.offset()-1, (uint8_t *)&RESPONSE);
                dbuf.reset();
                dbuf.bytes((uint8_t *)RESPONSE, sizeof(RESPONSE));
                dstr = dbuf.toString();
                kore_debug("RESP(%d)=%s", dbuf.offset()-1, dstr);

                if (strncmp(dstr, response, dbuf.offset()-1)) {
                    kore_debug("computed digest is invalid, %s != %s",
                               dstr, response);
                    kore_free(digest_str);
                    break;
                }
                ok = true;
            } while(0);

            return ok;
        }

#define _check_auth_type(t, a) (((t)==(a))||((t)==AUTH_TYPE_ANY))
        user_t authenticate(Request &req, AuthInfoCb getInfo, auth_type t) {
            user_t cred{nullptr, nullptr};
            cc_string auth_header = req.header("Authorization");
            if (!getInfo || !auth_header) return cred;

            if (_check_auth_type(t, AUTH_TYPE_BASIC) && strstr(auth_header, "Basic ")) {

                if (basic(auth_header, &cred.username, getInfo)) {
                    cred.authenticated = true;
                } else {
                    kore_debug("basic authentication failed");
                }
            } else if (_check_auth_type(t, AUTH_TYPE_DIGEST) && strstr(auth_header, "Digest ")) {

                Session* sess = req.session(false);
                if (sess != NULL && !sess->isNew() &&
                    digest(auth_header, &cred.username, req.requestMethod(), getInfo)) {
                    // Only validate information if there is already a session created for user
                    cred.authenticated = true;
                } else {
                    kore_debug("digest authentication failed");
                }
            }
            return cred;
        }

        bool authorize(Request& req,
                       Response& resp,
                       user_t& user,
                       cc_string realm,
                       auth_type type) {
            buffer sbuf(32);
            if (type == AUTH_TYPE_BASIC || type == AUTH_TYPE_ANY) {
                // request basic authentication
                sbuf.appendf("Basic realm=\"%s\"", realm);
                resp.header("WWW-Authenticate", sbuf.toString());
            } else {
                // request digest authentication
                uint8_t key[16], NOUNCE[MD5_DIGEST_LENGTH];

                if (!RAND_bytes(key, sizeof(key))) {
                    kore_debug("generating random bytes for digest authentication failed");
                    resp.status(INTERNAL_ERROR);
                    return false;
                }
                sbuf.appendf("Digest realm=\"%s\", ", realm);
                sbuf.appendf("nonce=\"");
                MD5((const uint8_t*)key, MD5_DIGEST_LENGTH, (uint8_t *) NOUNCE);
                sbuf.bytes(NOUNCE, sizeof(NOUNCE));
                sbuf.appendf("\", algorithm=MD5, opaque=\"");
                // our session is a base64 encoded random string
                sbuf.append(req.session()->id(), 2*MD5_DIGEST_LENGTH);
                sbuf.appendf("\"");
                resp.header("WWW-Authenticate", sbuf.toString());
            }
            return true;
        }
    }
}
