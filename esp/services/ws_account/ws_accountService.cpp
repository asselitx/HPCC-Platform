/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#include "ws_accountService.hpp"
#ifdef _USE_OPENLDAP
#include "ldapsecurity.ipp"
#endif
#include "exception_util.hpp"

const int CUTOFF_MAJOR = 533;
const int CUTOFF_MINOR = 6;

const char* MSG_SEC_MANAGER_IS_NULL = "Security manager is not found. Please check if the system authentication is set up correctly";

void Cws_accountEx::init(IPropertyTree *cfg, const char *process, const char *service)
{
}

#ifdef _USE_OPENLDAP
bool Cws_accountEx::onUpdateUser(IEspContext &context, IEspUpdateUserRequest & req, IEspUpdateUserResponse & resp)
{
    try
    {
        ILdapSecManager* secmgr = dynamic_cast<ILdapSecManager*>(context.querySecManager());
        if(secmgr == NULL)
        {
            throw MakeStringException(ECLWATCH_INVALID_SEC_MANAGER, "Security manager can't be converted to LdapSecManager. Only LdapSecManager supports this function.");
        }

        ISecUser* user = context.queryUser();
        if(user == NULL)
        {
            resp.setRetcode(-1);
            resp.setMessage("Can't find user in esp context. Please check if the user was properly logged in.");
            return false;
        }
        if(req.getUsername() == NULL || strcmp(req.getUsername(), user->getName()) != 0)
        {
            resp.setRetcode(-1);
            resp.setMessage("Username/password don't match.");
            return false;
        }

        const char* oldpass = req.getOldpass();
        if(oldpass == nullptr)
        {
            resp.setRetcode(-1);
            resp.setMessage("Current password must be provided.");
            return false;
        }

        const char* newpass1 = req.getNewpass1();
        const char* newpass2 = req.getNewpass2();
        if(newpass1 == NULL || newpass2 == NULL || strlen(newpass1) < 8 || strlen(newpass2) < 8)
        {
            resp.setRetcode(-1);
            resp.setMessage("New password must be 8 characters or longer.");
            return false;
        }

        if(strcmp(newpass1, newpass2) != 0)
        {
            resp.setRetcode(-1);
            resp.setMessage("Password and retype don't match.");
            return false;
        }

        if(strcmp(oldpass, newpass1) == 0)
        {
            resp.setRetcode(-1);
            resp.setMessage("New password can't be the same as current password.");
            return false;
        }

        const char* pwscheme = secmgr->getPasswordStorageScheme();
        bool isCrypt = pwscheme && (stricmp(pwscheme, "CRYPT") == 0);
        if(isCrypt && strncmp(oldpass, newpass1, 8) == 0)
        {
            resp.setRetcode(-1);
            resp.setMessage("The first 8 characters of the new password must be different from before.");
            return false;
        }

        SecFeatureSet sfs = secmgr->queryImplementedFeatures();
        if (!(sfs & SMF_UpdateUserPassword))
        {
            resp.setRetcode(-1);
            resp.setMessage("Changing password is not supported.");
            return false;
        }

        bool ok = false;
        try
        {
            ok = secmgr->updateUserPassword(*user, newpass1, oldpass, context.querySecureContext());
        }
        catch(IException* e)
        {
            StringBuffer emsg;
            e->errorMessage(emsg);
            resp.setRetcode(-1);
            resp.setMessage(emsg.str());
            return false;
        }
        catch(...)
        {
            ok = false;
        }

        if(!ok)
        {
            throw MakeStringException(ECLWATCH_CANNOT_CHANGE_PASSWORD, "Failed in changing password.");
        }

        resp.setRetcode(0);
        if(isCrypt && strlen(newpass1) > 8)
            resp.setMessage("Your password has been changed successfully, however, only the first 8 chars are effective.");
        else
            resp.setMessage("Your password has been changed successfully.");
    }
    catch(IException* e)
    {
        FORWARDEXCEPTION(context, e, ECLWATCH_INTERNAL_ERROR);
    }
    return true;
}

bool Cws_accountEx::onUpdateUserInput(IEspContext &context, IEspUpdateUserInputRequest &req, IEspUpdateUserInputResponse &resp)
{
    try
    {
        ISecUser* user = context.queryUser();
        if(user != NULL)
        {
            resp.setUsername(user->getName());
        }
    }
    catch(IException* e)
    {
        FORWARDEXCEPTION(context, e, ECLWATCH_INTERNAL_ERROR);
    }
    return true;
}

bool Cws_accountEx::onMyAccount(IEspContext &context, IEspMyAccountRequest &req, IEspMyAccountResponse &resp)
{
    try
    {
        ISecUser* userInContext = context.queryUser();
        ILdapSecManager* secmgr = dynamic_cast<ILdapSecManager*>(context.querySecManager());
        if (!userInContext)
        {
            if (secmgr || (context.getDomainAuthType() == AuthUserNameOnly))
                throw MakeStringException(ECLWATCH_INVALID_SEC_MANAGER, "User not set in EspContext");
            return true;
        }
        if (!secmgr)
        {
            resp.setUsername(userInContext->getName());
            return true;
        }
        double version = context.getClientVersion();
        const char* userName = userInContext->getName();
        Owned<ISecUser> user = secmgr->findUser(userName, context.querySecureContext());
        if(user != NULL)
        {
            CDateTime dt;
            user->getPasswordExpiration(dt);
            StringBuffer sb;
            if (dt.isNull())
            {
                sb.append("Never");
                if (version >= 1.04)
                    resp.setPasswordNeverExpires(true);
            }
            else
            {
                dt.getString(sb);
                sb.replace('T', (char)0);//chop off timestring
                if (version >= 1.04)
                    resp.setPasswordNeverExpires(false);
            }
            resp.setPasswordExpiration(sb.str());
            resp.setPasswordDaysRemaining(user->getPasswordDaysRemaining());
            resp.setFirstName(user->getFirstName());
            resp.setLastName(user->getLastName());
            resp.setUsername(user->getName());

            if (version >= 1.01)
                resp.setPasswordExpirationWarningDays(context.querySecManager()->getPasswordExpirationWarningDays(context.querySecureContext()));

            if (version >= 1.02)
                resp.setEmployeeID(user->getEmployeeID());

            if (version >= 1.03)
                resp.setDistinguishedName(user->getDistinguishedName());

            if (version >= 1.04)
            {
                resp.setAccountType(secmgr->isSuperUser(user) ? "Administrator" : "User");
                resp.setPasswordIsExpired(userInContext->getAuthenticateStatus() == AS_PASSWORD_EXPIRED || userInContext->getAuthenticateStatus() == AS_PASSWORD_VALID_BUT_EXPIRED);
            }

            if (version >= 1.05)
            {
                resp.setAccountStatus(userInContext->getAuthenticateStatus());
            }

            if (version >= 1.06)
            {
                SecFeatureSet sfs = secmgr->queryImplementedFeatures();
                resp.setCanUpdatePassword((sfs & SMF_UpdateUserPassword) ? true : false);
            }

            if (version >= 1.07)
            {
                StringArray groups;
                secmgr->getGroups(userName, groups);
                resp.setGroups(groups);
            }
        }
    }
    catch(IException* e)
    {
        FORWARDEXCEPTION(context, e, ECLWATCH_INTERNAL_ERROR);
    }
    return true;
}


#endif

bool Cws_accountEx::onVerifyUser(IEspContext &context, IEspVerifyUserRequest &req, IEspVerifyUserResponse &resp)
{
    try
    {
        ISecUser* usr = context.queryUser();
        if(!usr || usr->getAuthenticateStatus() != AS_AUTHENTICATED)
        {
            resp.setRetcode(-1);
            return false;
        }

        const char* ver = req.getVersion();
        if (!ver || !*ver)
        {
            throw MakeStringException(ECLWATCH_OLD_CLIENT_VERSION, "Client version not found");
        }

        int minor = 0;
        int major = 0;
        const char* dot1 = strrchr(ver, '.');
        if (!dot1)
            minor = atoi(ver);
        else if (strlen(dot1) > 1)
        {
            minor = atoi(dot1 + 1);
            if(dot1 > ver)
            {
                const char* dot2 = dot1 - 1;

                while(dot2 > ver && *dot2 != '.')
                    dot2--;
                if(*dot2 == '.')
                    dot2++;
                if(dot2 < dot1)
                {
                    StringBuffer majorstr;
                    majorstr.append(dot1 - dot2, dot2);
                    major = atoi(majorstr.str());
                }
            }
        }

        if(major > CUTOFF_MAJOR || (major == CUTOFF_MAJOR && minor >= CUTOFF_MINOR))
        {
            resp.setRetcode(0);
            return true;
        }

        const char* build_ver = getBuildVersion();
        if (build_ver && *build_ver)
            throw MakeStringException(ECLWATCH_OLD_CLIENT_VERSION, "Client version %s (server %s) is out of date.", ver, build_ver);
        else
            throw MakeStringException(ECLWATCH_OLD_CLIENT_VERSION, "Client version %s is out of date.", ver);
    }
    catch(IException* e)
    {
        FORWARDEXCEPTION(context, e, ECLWATCH_INTERNAL_ERROR);
    }

    return true;
}

