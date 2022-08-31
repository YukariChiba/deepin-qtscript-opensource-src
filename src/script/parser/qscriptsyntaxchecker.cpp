/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtScript module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qscriptsyntaxchecker_p.h"

#include "qscriptlexer_p.h"
#include "qscriptparser_p.h"

#include <stdlib.h>

QT_BEGIN_NAMESPACE

namespace QScript {


SyntaxChecker::SyntaxChecker():
    tos(0),
    stack_size(0),
    state_stack(0)
{
}

SyntaxChecker::~SyntaxChecker()
{
    if (stack_size) {
        free(state_stack);
    }
}

bool SyntaxChecker::automatic(QScript::Lexer *lexer, int token) const
{
    return token == T_RBRACE || token == 0 || lexer->prevTerminator();
}

SyntaxChecker::Result SyntaxChecker::checkSyntax(const QString &code)
{
  const int INITIAL_STATE = 0;
  QScript::Lexer lexer (/*engine=*/ 0);
  lexer.setCode(code, /*lineNo*/ 1);

  int yytoken = -1;
  int saved_yytoken = -1;
  QString error_message;
  int error_lineno = -1;
  int error_column = -1;
  State checkerState = Valid;

  reallocateStack();

  tos = 0;
  state_stack[++tos] = INITIAL_STATE;

  while (true)
    {
      const int state = state_stack [tos];
      if (yytoken == -1 && - TERMINAL_COUNT != action_index [state])
        {
          if (saved_yytoken == -1)
            yytoken = lexer.lex();
          else
            {
              yytoken = saved_yytoken;
              saved_yytoken = -1;
            }
        }

      int act = t_action (state, yytoken);

      if (act == ACCEPT_STATE) {
          if (lexer.error() == QScript::Lexer::UnclosedComment)
              checkerState = Intermediate;
          else
              checkerState = Valid;
          break;
      } else if (act > 0) {
          if (++tos == stack_size)
            reallocateStack();

          state_stack [tos] = act;
          yytoken = -1;
        }

      else if (act < 0)
        {
          int r = - act - 1;

          tos -= rhs [r];
          act = state_stack [tos++];

          if ((r == Q_SCRIPT_REGEXPLITERAL_RULE1)
              || (r == Q_SCRIPT_REGEXPLITERAL_RULE2)) {
              // Skip the rest of the RegExp literal
              bool rx = lexer.scanRegExp();
              if (!rx) {
                  checkerState = Intermediate;
                  break;
              }
          }

          state_stack [tos] = nt_action (act, lhs [r] - TERMINAL_COUNT);
        }

      else
        {
          if (saved_yytoken == -1 && automatic (&lexer, yytoken) && t_action (state, T_AUTOMATIC_SEMICOLON) > 0)
            {
              saved_yytoken = yytoken;
              yytoken = T_SEMICOLON;
              continue;
            }

          else if ((state == INITIAL_STATE) && (yytoken == 0)) {
              // accept empty input
              yytoken = T_SEMICOLON;
              continue;
          }

          int ers = state;
          int shifts = 0;
          int reduces = 0;
          int expected_tokens [3];
          for (int tk = 0; tk < TERMINAL_COUNT; ++tk)
            {
              int k = t_action (ers, tk);

              if (! k)
                continue;
              else if (k < 0)
                ++reduces;
              else if (spell [tk])
                {
                  if (shifts < 3)
                    expected_tokens [shifts] = tk;
                  ++shifts;
                }
            }

          error_message.clear ();
          if (shifts && shifts < 3)
            {
              bool first = true;

              for (int s = 0; s < shifts; ++s)
                {
                  if (first)
                    error_message += QLatin1String ("Expected ");
                  else
                    error_message += QLatin1String (", ");

                  first = false;
                  error_message += QLatin1Char('`');
                  error_message += QLatin1String (spell [expected_tokens [s]]);
                  error_message += QLatin1Char('\'');
                }
            }

          if (error_message.isEmpty())
              error_message = lexer.errorMessage();

          error_lineno = lexer.startLineNo();
          error_column = lexer.startColumnNo();
          checkerState = Error;
          break;
        }
    }

  if (checkerState == Error) {
      if (lexer.error() == QScript::Lexer::UnclosedComment)
          checkerState = Intermediate;
      else if (yytoken == 0)
          checkerState = Intermediate;
  }
  return Result(checkerState, error_lineno, error_column, error_message);
}

} // namespace QScript

QT_END_NAMESPACE
