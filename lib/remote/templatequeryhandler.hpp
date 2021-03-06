/* Icinga 2 | (c) 2012 Icinga GmbH | GPLv2+ */

#ifndef TEMPLATEQUERYHANDLER_H
#define TEMPLATEQUERYHANDLER_H

#include "remote/httphandler.hpp"

namespace icinga
{

class TemplateQueryHandler final : public HttpHandler
{
public:
	DECLARE_PTR_TYPEDEFS(TemplateQueryHandler);

	bool HandleRequest(const ApiUser::Ptr& user, HttpRequest& request,
		HttpResponse& response, const Dictionary::Ptr& params) override;
};

}

#endif /* TEMPLATEQUERYHANDLER_H */
