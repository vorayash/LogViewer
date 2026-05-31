#ifndef CHART_TEMPLATE_H
#define CHART_TEMPLATE_H

#include <string>

namespace LogAnalyzer {

class ChartTemplate {
public:
    static std::string generateHTML(
        const std::string& levelDataJson,
        const std::string& timelineDataJson
    );
    
private:
    static const char* getHTMLTemplate();
};

} // namespace LogAnalyzer

#endif // CHART_TEMPLATE_H

// Made with Bob
