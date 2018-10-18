/*
    Copyright 2018 Brick

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software
    and associated documentation files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or
    substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
    BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
    DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <mem/jit_pattern.h>
#include <asmjit/asmjit.h>

namespace mem
{
    jit_runtime::jit_runtime()
        : context_(new asmjit::JitRuntime())
    { }

    jit_runtime::~jit_runtime()
    {
        delete static_cast<asmjit::JitRuntime*>(context_);
    }

    jit_pattern::jit_pattern(jit_runtime* runtime, const pattern& pattern)
        : runtime_(runtime)
        , scanner_(runtime_->compile(pattern))
    { }

    jit_pattern::~jit_pattern()
    {
        if (runtime_ && scanner_)
        {
            runtime_->release(scanner_);
        }
    }

    jit_pattern::jit_pattern(jit_pattern&& rhs)
    {
        runtime_ = rhs.runtime_;
        scanner_ = rhs.scanner_;

        rhs.runtime_ = nullptr;
        rhs.scanner_ = nullptr;
    }

    scanner_func jit_runtime::compile(const pattern& pattern)
    {
        asmjit::JitRuntime* runtime = static_cast<asmjit::JitRuntime*>(context_);

        const size_t trimmed_size = pattern.trimmed_size();

        if (!trimmed_size)
        {
            return nullptr;
        }

        const byte* bytes = pattern.bytes();
        const byte* masks = pattern.masks();

        using namespace asmjit;

        CodeHolder code;
        code.init(runtime->getCodeInfo());

        X86Compiler cc(&code);
        cc.addFunc(FuncSignatureT<const void*, const void*, const void*>());

        X86Gp V_Current = cc.newUIntPtr("Current");
        X86Gp V_End     = cc.newUIntPtr("End");
        X86Gp V_Temp    = cc.newUIntPtr("Temp");
        X86Gp V_Temp8   = V_Temp.r8();
        X86Gp V_Temp32  = V_Temp.r32();
        X86Gp V_SkipTable;

        Label L_ScanLoop = cc.newLabel();
        Label L_NotFound = cc.newLabel();
        Label L_Next     = cc.newLabel();

        cc.setArg(0, V_Current);
        cc.setArg(1, V_End);

        const size_t original_size = pattern.size();

        cc.sub(V_End, (uint64_t) original_size);

        const size_t* skips = pattern.bad_char_skips();

        if (skips && ASMJIT_ARCH_X64)
        {
            V_SkipTable = cc.newUIntPtr();

            cc.mov(V_SkipTable, (uint64_t) skips);
        }

        cc.jmp(L_ScanLoop);

        cc.bind(L_Next);

        if (skips)
        {
            const size_t skip_pos = pattern.skip_pos();

            cc.movzx(V_Temp32, x86::byte_ptr(V_Current, static_cast<int32_t>(skip_pos)));

            if (ASMJIT_ARCH_X64)
            {
                cc.add(V_Current, x86::qword_ptr(V_SkipTable, V_Temp32, 3));
            }
            else /*if (ASMJIT_ARCH_X86)*/
            {
                cc.add(V_Current, x86::dword_ptr((uint64_t) skips, V_Temp32, 2));
            }
        }
        else
        {
            cc.add(V_Current, 1);
        }

        cc.bind(L_ScanLoop);
        cc.cmp(V_Current, V_End);
        cc.ja(L_NotFound);

        for (size_t i = trimmed_size; i--;)
        {
            const uint8_t byte = bytes[i];
            const uint8_t mask = masks[i];

            if (mask != 0)
            {
                if (mask == 0xFF)
                {
                    cc.cmp(x86::byte_ptr(V_Current, static_cast<int32_t>(i)), byte);
                }
                else
                {
                    cc.mov(V_Temp8, x86::byte_ptr(V_Current, static_cast<int32_t>(i)));
                    cc.and_(V_Temp8, mask);
                    cc.cmp(V_Temp8, byte);
                }

                cc.jne(L_Next);
            }
        }

        cc.ret(V_Current);

        cc.bind(L_NotFound);
        cc.xor_(V_Current, V_Current);
        cc.ret(V_Current);

        cc.endFunc();
        cc.finalize();

        scanner_func result = nullptr;

        Error err = runtime->add(&result, &code);

        if (err && result)
        {
            release(result);

            result = nullptr;
        }

        return result;
    }

    void jit_runtime::release(scanner_func scanner)
    {
        asmjit::JitRuntime* runtime = static_cast<asmjit::JitRuntime*>(context_);

        if (scanner)
        {
            runtime->release(scanner);
        }
    }
}
