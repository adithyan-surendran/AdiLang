#include "compiler.h"
#include "vm.h"

#include "modules/fs.h"
#include "modules/stdio.h"
#include "modules/math.h"
#include "modules/os.h"
#include "modules/string.h"
#include "modules/time.h"
#include "modules/random.h"
#include "modules/json.h"
#include "modules/array.h"

#include <stdexcept>
#include <iostream>


// ============================================================
// Native Modules
// ============================================================

Value Compiler::loadNativeModule(
    const std::string& name
)
{
    if (name == "stdio") {
        return createIOModule(vm);
    }
    else if (name == "math") {
        return createMathModule(vm);
    }
    else if (name == "string") {
        return createStringModule(vm);
    }
    else if (name == "os") {
        return createOSModule(vm);
    }
    else if (name == "time") {
        return createTimeModule(vm);
    }
    else if (name == "random") {
        return createRandomModule(vm);
    }
    else if (name == "json") {
        return createJsonModule(vm);
    }
    else if (name == "fs") {
        return createFSModule(vm);
    }
    else if (name == "array"){
        return createArrayModule(vm);
    }

    throw std::runtime_error(
        "Compiler Error: Unknown module '@import " +
        name +
        "'."
    );
}


// ============================================================
// Constructor
// ============================================================

Compiler::Compiler(
    VM* vm
)
    : vm(vm)
{
}


// ============================================================
// Compile Program
// ============================================================

bool Compiler::compile(
    const Program* program,
    Chunk* chunk
)
{
    AdiFunction* scriptFunction =
        vm->allocateObject<AdiFunction>(
            "script"
        );

    scriptFunction->chunk =
        std::make_shared<Chunk>(
            *chunk
        );

    initFunction(
        scriptFunction,
        FunctionType::TYPE_SCRIPT
    );

    try {

        for (
            const auto& stmt :
            program->statements
        ) {
            compileNode(
                stmt.get()
            );
        }

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_NIL
            ),
            0
        );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_RETURN
            ),
            0
        );

        AdiFunction* compiledFunction =
            endCompiler();

        *chunk =
            *compiledFunction->chunk;

        return true;
    }
    catch (
        const std::runtime_error& e
    ) {

        std::cerr
            << "Compiler Error: "
            << e.what()
            << "\n";

        if (current) {
            delete current;
            current = nullptr;
        }

        return false;
    }
}


// ============================================================
// Compile Statements
// ============================================================

void Compiler::compileNode(
    const Stmt* stmt
)
{
    int line =
        stmt->line;


    // ========================================================
    // Variable Declaration
    // ========================================================

    if (
        auto varDecl =
            dynamic_cast<
                const VariableDeclaration*
            >(stmt)
    )
    {
        compileExpression(
            varDecl->initializer.get()
        );

        if (
            current->scopeDepth > 0
        )
        {
            parseVariable(
                varDecl->name
            );

            markInitialized();
        }
        else
        {
            uint8_t global =
                parseVariable(
                    varDecl->name
                );

            defineVariable(
                global,
                line
            );
        }
    }


    // ========================================================
    // Import
    // ========================================================

    else if (
        auto importStmt =
            dynamic_cast<
                const ImportStmt*
            >(stmt)
    )
    {
        Value moduleObj =
            loadNativeModule(
                importStmt->moduleName
            );


        // ----------------------------------------------------
        // stdio
        // ----------------------------------------------------

        if (
            importStmt->moduleName ==
            "stdio"
        )
        {
            AdiInstance* moduleInstance =
                std::get<AdiInstance*>(
                    moduleObj
                );

            for (
                const auto& pair :
                moduleInstance->fields
            )
            {
                uint8_t nameConst =
                    makeConstant(
                        pair.first
                    );

                emitConstant(
                    pair.second
                );

                defineVariable(
                    nameConst,
                    line
                );
            }
        }


        // ----------------------------------------------------
        // Other modules
        // ----------------------------------------------------

        else
        {
            uint8_t nameConst =
                makeConstant(
                    importStmt->moduleName
                );

            emitConstant(
                moduleObj
            );

            defineVariable(
                nameConst,
                line
            );
        }
    }


    // ========================================================
    // Print
    // ========================================================

    else if (
        auto printStmt =
            dynamic_cast<
                const PrintStatement*
            >(stmt)
    )
    {
        compileExpression(
            printStmt->expression.get()
        );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_PRINT
            ),
            line
        );
    }


    // ========================================================
    // Expression Statement
    // ========================================================

    else if (
        auto exprStmt =
            dynamic_cast<
                const ExpressionStatement*
            >(stmt)
    )
    {
        compileExpression(
            exprStmt->expression.get()
        );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_POP
            ),
            line
        );
    }


    // ========================================================
    // Block
    // ========================================================

    else if (
        auto blockStmt =
            dynamic_cast<
                const BlockStatement*
            >(stmt)
    )
    {
        beginScope();

        for (
            const auto& s :
            blockStmt->statements
        )
        {
            compileNode(
                s.get()
            );
        }

        endScope(
            line
        );
    }


    // ========================================================
    // If / Else
    // ========================================================

    else if (
        auto ifStmt =
            dynamic_cast<
                const IfStatement*
            >(stmt)
    )
    {
        compileExpression(
            ifStmt->condition.get()
        );

        int thenJump =
            emitJump(
                static_cast<uint8_t>(
                    OpCode::OP_JUMP_IF_FALSE
                )
            );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_POP
            ),
            line
        );

        compileNode(
            ifStmt->thenBranch.get()
        );

        int elseJump =
            emitJump(
                static_cast<uint8_t>(
                    OpCode::OP_JUMP
                )
            );

        patchJump(
            thenJump
        );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_POP
            ),
            line
        );

        if (
            ifStmt->elseBranch != nullptr
        )
        {
            compileNode(
                ifStmt->elseBranch.get()
            );
        }

        patchJump(
            elseJump
        );
    }


    // ========================================================
    // While
    // ========================================================

    else if (
        auto whileStmt =
            dynamic_cast<
                const WhileStatement*
            >(stmt)
    )
    {
        int loopStart =
            static_cast<int>(
                currentChunk()->code.size()
            );

        compileExpression(
            whileStmt->condition.get()
        );

        int exitJump =
            emitJump(
                static_cast<uint8_t>(
                    OpCode::OP_JUMP_IF_FALSE
                )
            );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_POP
            ),
            line
        );

        compileNode(
            whileStmt->body.get()
        );

        emitLoop(
            loopStart
        );

        patchJump(
            exitJump
        );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_POP
            ),
            line
        );
    }


    // ========================================================
    // Function Declaration
    // ========================================================

    else if (
        auto funcStmt =
            dynamic_cast<
                const FunctionStatement*
            >(stmt)
    )
    {
        // ----------------------------------------------------
        // Determine whether this is a local function.
        // ----------------------------------------------------

        bool isLocal =
            current->scopeDepth > 0;

        uint8_t localSlot = 0;


        // ----------------------------------------------------
        // Reserve local function name BEFORE compiling body.
        //
        // This is what allows:
        //
        // fn countDown(n) {
        //     return countDown(n - 1);
        // }
        //
        // The name already exists in the enclosing scope,
        // so resolveUpvalue() can find it.
        // ----------------------------------------------------

        if (isLocal)
        {
            declareVariable(
                funcStmt->name
            );

            localSlot =
                static_cast<uint8_t>(
                    current->locals.size() - 1
                );

            /*
             * Mark initialized for compiler resolution.
             *
             * The closure will be placed into this slot
             * after it has been created.
             */
            markInitialized();
        }


        // ----------------------------------------------------
        // Create function object
        // ----------------------------------------------------

        AdiFunction* function =
            vm->allocateObject<AdiFunction>(
                funcStmt->name,
                funcStmt
            );

        function->arity =
            static_cast<int>(
                funcStmt->params.size()
            );


        // ----------------------------------------------------
        // Save enclosing compiler
        // ----------------------------------------------------

        FunctionCompiler* enclosing =
            current;


        // ----------------------------------------------------
        // Create child compiler
        // ----------------------------------------------------

        initFunction(
            function,
            FunctionType::TYPE_FUNCTION
        );


        // ----------------------------------------------------
        // Function scope
        // ----------------------------------------------------

        beginScope();


        // ----------------------------------------------------
        // Parameters
        //
        // Slot 0 = callee
        // Slot 1+ = parameters
        // ----------------------------------------------------

        for (
            const auto& param :
            funcStmt->params
        )
        {
            declareVariable(
                param
            );

            markInitialized();
        }


        // ----------------------------------------------------
        // Function body
        // ----------------------------------------------------

        for (
            const auto& s :
            funcStmt->body->statements
        )
        {
            compileNode(
                s.get()
            );
        }


        // ----------------------------------------------------
        // Save upvalues before endCompiler()
        // ----------------------------------------------------

        std::vector<CompilerUpvalue> upvalues =
            current->upvalues;

        function->upvalueCount =
            static_cast<int>(
                upvalues.size()
            );


        // ----------------------------------------------------
        // Finish child compiler
        // ----------------------------------------------------

        AdiFunction* compiledFunction =
            endCompiler();


        // ----------------------------------------------------
        // current is now the enclosing compiler.
        //
        // IMPORTANT:
        // Do NOT call parseVariable() here for a local
        // function. Its slot was already reserved above.
        // ----------------------------------------------------


        // ----------------------------------------------------
        // Emit closure
        // ----------------------------------------------------

        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_CLOSURE
            ),
            makeConstant(
                compiledFunction
            ),
            line
        );


        // ----------------------------------------------------
        // Emit upvalue descriptors
        // ----------------------------------------------------

        for (
            const auto& upvalue :
            upvalues
        )
        {
            emitByte(
                upvalue.isLocal
                    ? 1
                    : 0,
                line
            );

            emitByte(
                upvalue.index,
                line
            );
        }


        // ----------------------------------------------------
        // Define function
        // ----------------------------------------------------

        if (isLocal)
        {
            /*
             * The closure is now on top of the stack and
             * belongs to the previously reserved local slot.
             */
            defineVariable(
                localSlot,
                line
            );
        }
        else
        {
            /*
             * Global functions don't have a local slot.
             */
            uint8_t global =
                parseVariable(
                    funcStmt->name
                );

            defineVariable(
                global,
                line
            );
        }
    }


    // ========================================================
    // Return
    // ========================================================

    else if (
        auto returnStmt =
            dynamic_cast<
                const ReturnStatement*
            >(stmt)
    )
    {
        if (
            returnStmt->value != nullptr
        )
        {
            compileExpression(
                returnStmt->value.get()
            );
        }
        else
        {
            emitByte(
                static_cast<uint8_t>(
                    OpCode::OP_NIL
                ),
                line
            );
        }

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_RETURN
            ),
            line
        );
    }


    // ========================================================
    // Struct / Class
    // ========================================================

    else if (
        auto structStmt =
            dynamic_cast<
                const StructStmt*
            >(stmt)
    )
    {
        uint8_t nameConst =
            makeConstant(
                structStmt->name
            );


        // ----------------------------------------------------
        // Find superclass
        // ----------------------------------------------------

        AdiStructDef* parentDef =
            nullptr;

        if (
            structStmt->superclass.has_value()
        )
        {
            std::string superclassName =
                structStmt->superclass.value();

            if (
                definedStructs.find(
                    superclassName
                ) ==
                definedStructs.end()
            )
            {
                throw std::runtime_error(
                    "Compiler Error: Undefined superclass '" +
                    superclassName +
                    "'."
                );
            }

            parentDef =
                definedStructs[
                    superclassName
                ];
        }


        // ----------------------------------------------------
        // Combine inherited fields
        // ----------------------------------------------------

        std::vector<std::string>
            combinedFields;

        if (
            parentDef != nullptr
        )
        {
            combinedFields =
                parentDef->fields;
        }

        for (
            const auto& field :
            structStmt->fields
        )
        {
            combinedFields.push_back(
                field
            );
        }


        // ----------------------------------------------------
        // Create struct definition
        // ----------------------------------------------------

        AdiStructDef* structDef =
            vm->allocateObject<AdiStructDef>(
                structStmt->name,
                combinedFields,
                parentDef
            );


        // ----------------------------------------------------
        // Inherit methods
        // ----------------------------------------------------

        if (
            parentDef != nullptr
        )
        {
            structDef->methods =
                parentDef->methods;
        }


        // ----------------------------------------------------
        // Enter class compiler
        // ----------------------------------------------------

        ClassCompiler classCompiler;

        classCompiler.enclosing =
            currentClass;

        classCompiler.hasSuperclass =
            structStmt->superclass.has_value();

        if (
            structStmt->superclass.has_value()
        )
        {
            classCompiler.superclassName =
                structStmt->superclass.value();
        }

        currentClass =
            &classCompiler;


        // ----------------------------------------------------
        // Compile methods
        // ----------------------------------------------------

        for (
            const auto& methodStmt :
            structStmt->methods
        )
        {
            AdiFunction* function =
                vm->allocateObject<AdiFunction>(
                    methodStmt->name,
                    methodStmt.get()
                );

            function->arity =
                static_cast<int>(
                    methodStmt->params.size()
                );


            // ------------------------------------------------
            // Save enclosing compiler
            // ------------------------------------------------

            FunctionCompiler* enclosing =
                current;


            // ------------------------------------------------
            // Initialize method compiler
            // ------------------------------------------------

            initFunction(
                function,
                FunctionType::TYPE_METHOD
            );


            // ------------------------------------------------
            // Method scope
            // ------------------------------------------------

            beginScope();


            // ------------------------------------------------
            // Parameters
            // ------------------------------------------------

            for (
                const auto& param :
                methodStmt->params
            )
            {
                declareVariable(
                    param
                );

                markInitialized();
            }


            // ------------------------------------------------
            // Method body
            // ------------------------------------------------

            for (
                const auto& s :
                methodStmt->body->statements
            )
            {
                compileNode(
                    s.get()
                );
            }


            // ------------------------------------------------
            // Finish method
            // ------------------------------------------------

            AdiFunction* compiledFunction =
                endCompiler();

            current =
                enclosing;


            // ------------------------------------------------
            // Store method
            // ------------------------------------------------

            structDef->methods[
                methodStmt->name
            ] =
                compiledFunction;
        }


        // ----------------------------------------------------
        // Exit class compiler
        // ----------------------------------------------------

        currentClass =
            currentClass->enclosing;


        // ----------------------------------------------------
        // Register struct
        // ----------------------------------------------------

        definedStructs[
            structStmt->name
        ] =
            structDef;


        // ----------------------------------------------------
        // Push struct definition
        // ----------------------------------------------------

        emitConstant(
            structDef
        );

        defineVariable(
            nameConst,
            line
        );
    }
}


// ============================================================
// Compile Expressions
// ============================================================

void Compiler::compileExpression(
    const Expr* expr
)
{
    int line =
        expr->line;


    // ========================================================
    // Number
    // ========================================================

    if (
        auto num =
            dynamic_cast<
                const NumberExpr*
            >(expr)
    )
    {
        emitConstant(
            num->value
        );
    }


    // ========================================================
    // String
    // ========================================================

    else if (
        auto str =
            dynamic_cast<
                const StringExpr*
            >(expr)
    )
    {
        emitConstant(
            str->value
        );
    }


    // ========================================================
    // Unary
    // ========================================================

    else if (
        auto unary =
            dynamic_cast<
                const UnaryExpr*
            >(expr)
    )
    {
        compileExpression(
            unary->right.get()
        );

        switch (
            unary->op
        )
        {
            case TokenType::MINUS:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_NEGATE
                    ),
                    line
                );

                break;

            case TokenType::BANG:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_NOT
                    ),
                    line
                );

                break;

            default:
                break;
        }
    }


    // ========================================================
    // Variable
    // ========================================================

    else if (
        auto var =
            dynamic_cast<
                const VariableExpr*
            >(expr)
    )
    {
        if (
            var->name == "true"
        )
        {
            emitByte(
                static_cast<uint8_t>(
                    OpCode::OP_TRUE
                ),
                line
            );
        }
        else if (
            var->name == "false"
        )
        {
            emitByte(
                static_cast<uint8_t>(
                    OpCode::OP_FALSE
                ),
                line
            );
        }
        else
        {
            int arg =
                resolveLocal(
                    current,
                    var->name
                );

            if (
                arg != -1
            )
            {
                emitBytes(
                    static_cast<uint8_t>(
                        OpCode::OP_GET_LOCAL
                    ),
                    static_cast<uint8_t>(
                        arg
                    ),
                    line
                );
            }
            else if (
                (arg =
                    resolveUpvalue(
                        current,
                        var->name
                    )) != -1
            )
            {
                emitBytes(
                    static_cast<uint8_t>(
                        OpCode::OP_GET_UPVALUE
                    ),
                    static_cast<uint8_t>(
                        arg
                    ),
                    line
                );
            }
            else
            {
                emitBytes(
                    static_cast<uint8_t>(
                        OpCode::OP_GET_GLOBAL
                    ),
                    makeConstant(
                        var->name
                    ),
                    line
                );
            }
        }
    }


    // ========================================================
    // Assignment
    // ========================================================

    else if (
        auto assign =
            dynamic_cast<
                const AssignExpr*
            >(expr)
    )
    {
        compileExpression(
            assign->value.get()
        );

        int arg =
            resolveLocal(
                current,
                assign->name
            );

        if (
            arg != -1
        )
        {
            emitBytes(
                static_cast<uint8_t>(
                    OpCode::OP_SET_LOCAL
                ),
                static_cast<uint8_t>(
                    arg
                ),
                line
            );
        }
        else if (
            (arg =
                resolveUpvalue(
                    current,
                    assign->name
                )) != -1
        )
        {
            emitBytes(
                static_cast<uint8_t>(
                    OpCode::OP_SET_UPVALUE
                ),
                static_cast<uint8_t>(
                    arg
                ),
                line
            );
        }
        else
        {
            emitBytes(
                static_cast<uint8_t>(
                    OpCode::OP_SET_GLOBAL
                ),
                makeConstant(
                    assign->name
                ),
                line
            );
        }
    }


    // ========================================================
    // Binary
    // ========================================================

    else if (
        auto bin =
            dynamic_cast<
                const BinaryExpr*
            >(expr)
    )
    {
        compileExpression(
            bin->left.get()
        );

        compileExpression(
            bin->right.get()
        );

        switch (
            bin->op
        )
        {
            case TokenType::PLUS:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_ADD
                    ),
                    line
                );

                break;

            case TokenType::MINUS:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_SUBTRACT
                    ),
                    line
                );

                break;

            case TokenType::STAR:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_MULTIPLY
                    ),
                    line
                );

                break;

            case TokenType::SLASH:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_DIVIDE
                    ),
                    line
                );

                break;

            case TokenType::EQUAL_EQUAL:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_EQUAL
                    ),
                    line
                );

                break;

            case TokenType::BANG_EQUAL:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_EQUAL
                    ),
                    line
                );

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_NOT
                    ),
                    line
                );

                break;

            case TokenType::GREATER:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_GREATER
                    ),
                    line
                );

                break;

            case TokenType::GREATER_EQUAL:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_LESS
                    ),
                    line
                );

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_NOT
                    ),
                    line
                );

                break;

            case TokenType::LESS:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_LESS
                    ),
                    line
                );

                break;

            case TokenType::LESS_EQUAL:

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_GREATER
                    ),
                    line
                );

                emitByte(
                    static_cast<uint8_t>(
                        OpCode::OP_NOT
                    ),
                    line
                );

                break;

            default:

                throw std::runtime_error(
                    "Compiler Error: Unsupported binary operator."
                );
        }
    }


    // ========================================================
    // Function Call
    // ========================================================

    else if (
        auto callExpr =
            dynamic_cast<
                const CallExpr*
            >(expr)
    )
    {
        /*
         * Stack:
         *
         * [callee, arg1, arg2, ...]
         */

        compileExpression(
            callExpr->callee.get()
        );

        for (
            const auto& arg :
            callExpr->arguments
        )
        {
            compileExpression(
                arg.get()
            );
        }

        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_CALL
            ),
            static_cast<uint8_t>(
                callExpr->arguments.size()
            ),
            line
        );
    }


    // ========================================================
    // Struct Instance
    // ========================================================

    else if (
        auto structInst =
            dynamic_cast<
                const StructInstanceExpr*
            >(expr)
    )
    {
        int arg =
            resolveLocal(
                current,
                structInst->name
            );

        if (
            arg != -1
        )
        {
            emitBytes(
                static_cast<uint8_t>(
                    OpCode::OP_GET_LOCAL
                ),
                static_cast<uint8_t>(
                    arg
                ),
                line
            );
        }
        else
        {
            emitBytes(
                static_cast<uint8_t>(
                    OpCode::OP_GET_GLOBAL
                ),
                makeConstant(
                    structInst->name
                ),
                line
            );
        }

        for (
            const auto& argExpr :
            structInst->arguments
        )
        {
            compileExpression(
                argExpr.get()
            );
        }

        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_STRUCT_INSTANCE
            ),
            static_cast<uint8_t>(
                structInst->arguments.size()
            ),
            line
        );
    }


    // ========================================================
    // Property Get
    // ========================================================

    else if (
        auto getExpr =
            dynamic_cast<
                const GetExpr*
            >(expr)
    )
    {
        compileExpression(
            getExpr->object.get()
        );

        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_GET
            ),
            makeConstant(
                getExpr->name
            ),
            line
        );
    }


    // ========================================================
    // Property Set
    // ========================================================

    else if (
        auto setExpr =
            dynamic_cast<
                const SetExpr*
            >(expr)
    )
    {
        compileExpression(
            setExpr->value.get()
        );

        compileExpression(
            setExpr->object.get()
        );

        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_SET
            ),
            makeConstant(
                setExpr->name
            ),
            line
        );
    }


    // ========================================================
    // Super
    // ========================================================

    else if (
        auto superExpr =
            dynamic_cast<
                const SuperExpr*
            >(expr)
    )
    {
        if (
            currentClass == nullptr ||
            !currentClass->hasSuperclass
        )
        {
            throw std::runtime_error(
                "Compiler Error: Can't use 'super' "
                "outside of a subclass method."
            );
        }


        // ----------------------------------------------------
        // Get this
        // ----------------------------------------------------

        int thisSlot =
            resolveLocal(
                current,
                "this"
            );

        if (
            thisSlot != -1
        )
        {
            emitBytes(
                static_cast<uint8_t>(
                    OpCode::OP_GET_LOCAL
                ),
                static_cast<uint8_t>(
                    thisSlot
                ),
                line
            );
        }
        else
        {
            throw std::runtime_error(
                "Compiler Error: Internal error - "
                "'this' not found."
            );
        }


        // ----------------------------------------------------
        // Get superclass
        // ----------------------------------------------------

        int superArg =
            resolveLocal(
                current,
                currentClass->superclassName
            );

        if (
            superArg != -1
        )
        {
            emitBytes(
                static_cast<uint8_t>(
                    OpCode::OP_GET_LOCAL
                ),
                static_cast<uint8_t>(
                    superArg
                ),
                line
            );
        }
        else
        {
            emitBytes(
                static_cast<uint8_t>(
                    OpCode::OP_GET_GLOBAL
                ),
                makeConstant(
                    currentClass->superclassName
                ),
                line
            );
        }


        // ----------------------------------------------------
        // Arguments
        // ----------------------------------------------------

        for (
            const auto& arg :
            superExpr->arguments
        )
        {
            compileExpression(
                arg.get()
            );
        }


        // ----------------------------------------------------
        // OP_SUPER
        // ----------------------------------------------------

        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_SUPER
            ),
            makeConstant(
                superExpr->method
            ),
            line
        );

        emitByte(
            static_cast<uint8_t>(
                superExpr->arguments.size()
            ),
            line
        );
    }


    // ========================================================
    // Map
    // ========================================================

    else if (
        auto mapExpr =
            dynamic_cast<
                const MapExpr*
            >(expr)
    )
    {
        for (
            const auto& [key, valExpr] :
            mapExpr->entries
        )
        {
            emitConstant(
                key
            );

            compileExpression(
                valExpr.get()
            );
        }

        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_MAP
            ),
            static_cast<uint8_t>(
                mapExpr->entries.size()
            ),
            line
        );
    }


    // ========================================================
    // Array
    // ========================================================

    else if (
        auto arrExpr =
            dynamic_cast<
                const ArrayExpr*
            >(expr)
    )
    {
        for (
            const auto& element :
            arrExpr->elements
        )
        {
            compileExpression(
                element.get()
            );
        }

        emitBytes(
            static_cast<uint8_t>(
                OpCode::OP_ARRAY
            ),
            static_cast<uint8_t>(
                arrExpr->elements.size()
            ),
            line
        );
    }


    // ========================================================
    // Array / Map Index Get
    // ========================================================

    else if (
        auto indexGet =
            dynamic_cast<
                const IndexGetExpr*
            >(expr)
    )
    {
        compileExpression(
            indexGet->target.get()
        );

        compileExpression(
            indexGet->index.get()
        );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_INDEX_GET
            ),
            line
        );
    }


    // ========================================================
    // Array / Map Index Set
    // ========================================================

    else if (
        auto indexSet =
            dynamic_cast<
                const IndexSetExpr*
            >(expr)
    )
    {
        compileExpression(
            indexSet->value.get()
        );

        compileExpression(
            indexSet->target.get()
        );

        compileExpression(
            indexSet->index.get()
        );

        emitByte(
            static_cast<uint8_t>(
                OpCode::OP_INDEX_SET
            ),
            line
        );
    }
}