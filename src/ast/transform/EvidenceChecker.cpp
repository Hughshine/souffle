//
// Created by 17308 on 2025/6/25.
//
#include "ast/transform/EvidenceChecker.h"
#include "ast/Program.h"
#include "ast/Directive.h"
#include "ast/TranslationUnit.h"
#include "reports/ErrorReport.h"

namespace souffle::ast::transform {

bool EvidenceSemanticChecker::transform(TranslationUnit& translationUnit) {
    auto& program = translationUnit.getProgram();

    for (const Directive* directive : program.getDirectives()) {
        if (directive->getType() == DirectiveType::evidence) {
            if (!checkEvidence(directive, translationUnit)) {
                // 你可以在这里记录错误，或者做别的处理
            }
        }
    }
    return false;  // 本 Checker 不修改 AST，仅检查
}

bool EvidenceSemanticChecker::checkEvidence(const Directive* directive,  TranslationUnit& translationUnit) {
    // 示例检查逻辑：这里你可以访问 directive 的内容做具体语义验证

    // 获取错误报告器
    const ErrorReport& report = translationUnit.getErrorReport();

    // TODO: 你的具体检查逻辑
    // 例如，如果 directive 参数不符合预期，调用 report.addError("错误信息", directive->getSrcLoc());

    // 这里暂时返回 true，代表检查通过
    return true;
}

}
