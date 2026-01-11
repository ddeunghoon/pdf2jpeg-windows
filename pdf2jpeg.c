// pdf2jpeg.c
// - exe와 같은 폴더에 있는 모든 *.pdf를 찾는다.
// - 각 PDF의 모든 페이지를 JPEG로 렌더링해서 exe 폴더에 저장한다.
//
// 출력 파일명 예:
//   input.pdf  -> input_p0001.jpg, input_p0002.jpg, ...
//
// MuPDF 렌더링은 공식 예제의 fz_new_pixmap_from_page_number() 방식을 사용. :contentReference[oaicite:1]{index=1}
// JPEG 저장은 fz_save_pixmap_as_jpeg() 사용. :contentReference[oaicite:2]{index=2}

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mupdf/fitz.h>

static const int   kJpegQuality = 90;     // 1~100
static const float kDpi         = 200.0f; // 렌더링 DPI (원하면 150/300 등으로)

static wchar_t* get_exe_dir_w(void)
{
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return NULL;

    wchar_t* last = wcsrchr(path, L'\\');
    if (!last) return NULL;
    *last = L'\0';

    size_t len = wcslen(path);
    wchar_t* out = (wchar_t*)calloc(len + 1, sizeof(wchar_t));
    if (!out) return NULL;
    wcscpy(out, path);
    return out;
}

static char* utf8_from_wide(const wchar_t* w)
{
    if (!w) return NULL;
    int need = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (need <= 0) return NULL;

    char* s = (char*)malloc((size_t)need);
    if (!s) return NULL;

    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, s, need, NULL, NULL))
    {
        free(s);
        return NULL;
    }
    return s;
}

static void make_output_name_w(const wchar_t* pdf_filename_only, int page_index0, wchar_t* out, size_t out_cap)
{
    // pdf_filename_only: "abc.pdf" 같은 파일명만 들어온다고 가정
    // out: "abc_p0001.jpg" 형태로 만든다.
    wchar_t base[MAX_PATH];
    wcsncpy(base, pdf_filename_only, MAX_PATH - 1);
    base[MAX_PATH - 1] = 0;

    wchar_t* dot = wcsrchr(base, L'.');
    if (dot) *dot = 0;

    // page_index0는 0-based. 파일명은 1-based로 표기.
    _snwprintf(out, out_cap, L"%s_p%04d.jpg", base, page_index0 + 1);
    out[out_cap - 1] = 0;
}

static int convert_one_pdf(fz_context* ctx, const wchar_t* pdf_fullpath_w, const wchar_t* exe_dir_w)
{
    int ok = 0;
    char* pdf_fullpath_u8 = utf8_from_wide(pdf_fullpath_w);
    if (!pdf_fullpath_u8) return 0;

    fz_document* doc = NULL;
    int page_count = 0;

    // DPI -> MuPDF scale matrix (기본 72dpi 기준) :contentReference[oaicite:3]{index=3}
    float scale = kDpi / 72.0f;
    fz_matrix ctm = fz_scale(scale, scale);

    fz_try(ctx)
    {
        doc = fz_open_document(ctx, pdf_fullpath_u8);
        page_count = fz_count_pages(ctx, doc);
    }
    fz_catch(ctx)
    {
        fz_report_error(ctx);
        fprintf(stderr, "ERROR: cannot open/count pages: %s\n", pdf_fullpath_u8);
        goto cleanup;
    }

    // 파일명만 뽑기(출력 파일 이름 만들 때 사용)
    const wchar_t* pdf_filename_only = wcsrchr(pdf_fullpath_w, L'\\');
    pdf_filename_only = pdf_filename_only ? (pdf_filename_only + 1) : pdf_fullpath_w;

    for (int i = 0; i < page_count; i++)
    {
        fz_pixmap* pix = NULL;

        fz_try(ctx)
        {
            // 공식 예제에서 쓰는 편의 함수: 페이지 -> RGB pixmap 렌더링 :contentReference[oaicite:4]{index=4}
            pix = fz_new_pixmap_from_page_number(ctx, doc, i, ctm, fz_device_rgb(ctx), 0);
        }
        fz_catch(ctx)
        {
            fz_report_error(ctx);
            fprintf(stderr, "WARN: render failed: %s (page %d/%d)\n", pdf_fullpath_u8, i + 1, page_count);
            if (pix) { fz_drop_pixmap(ctx, pix); pix = NULL; }
            continue;
        }

        // 출력 경로: exe_dir\{base}_p0001.jpg
        wchar_t out_name_only[MAX_PATH];
        make_output_name_w(pdf_filename_only, i, out_name_only, MAX_PATH);

        wchar_t out_fullpath_w[MAX_PATH];
        _snwprintf(out_fullpath_w, MAX_PATH, L"%s\\%s", exe_dir_w, out_name_only);
        out_fullpath_w[MAX_PATH - 1] = 0;

        char* out_fullpath_u8 = utf8_from_wide(out_fullpath_w);
        if (!out_fullpath_u8)
        {
            fprintf(stderr, "WARN: path conversion failed, skip saving.\n");
            fz_drop_pixmap(ctx, pix);
            continue;
        }

        fz_try(ctx)
        {
            // JPEG 저장 (quality만 받는 간편 함수) :contentReference[oaicite:5]{index=5}
            fz_save_pixmap_as_jpeg(ctx, pix, out_fullpath_u8, kJpegQuality);
        }
        fz_catch(ctx)
        {
            fz_report_error(ctx);
            fprintf(stderr, "WARN: save jpeg failed: %s\n", out_fullpath_u8);
        }

        free(out_fullpath_u8);
        fz_drop_pixmap(ctx, pix);
    }

    ok = 1;

cleanup:
    if (doc) fz_drop_document(ctx, doc);
    free(pdf_fullpath_u8);
    return ok;
}

int wmain(void)
{
    int any = 0;

    // 콘솔을 UTF-8 코드 페이지로 전환해 한글 파일명을 그대로 표시
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _wsetlocale(LC_ALL, L"");

    wchar_t* exe_dir_w = get_exe_dir_w();
    if (!exe_dir_w)
    {
        fprintf(stderr, "ERROR: cannot get exe directory.\n");
        return 1;
    }

    // MuPDF 컨텍스트 생성 + 핸들러 등록 (공식 예제 흐름) :contentReference[oaicite:6]{index=6}
    fz_context* ctx = fz_new_context(NULL, NULL, 256 * 1024 * 1024);
    if (!ctx)
    {
        fprintf(stderr, "ERROR: cannot create MuPDF context.\n");
        free(exe_dir_w);
        return 1;
    }

    fz_try(ctx)
        fz_register_document_handlers(ctx);
    fz_catch(ctx)
    {
        fz_report_error(ctx);
        fprintf(stderr, "ERROR: cannot register document handlers.\n");
        fz_drop_context(ctx);
        free(exe_dir_w);
        return 1;
    }

    // exe 폴더의 *.pdf 검색
    wchar_t pattern[MAX_PATH];
    _snwprintf(pattern, MAX_PATH, L"%s\\*.pdf", exe_dir_w);
    pattern[MAX_PATH - 1] = 0;

    WIN32_FIND_DATAW ffd;
    HANDLE h = FindFirstFileW(pattern, &ffd);
    if (h == INVALID_HANDLE_VALUE)
    {
        printf("No PDF files found in: %ls\n", exe_dir_w);
        fz_drop_context(ctx);
        free(exe_dir_w);
        return 0;
    }

    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        // full path = exe_dir\filename.pdf
        wchar_t pdf_fullpath_w[MAX_PATH];
        _snwprintf(pdf_fullpath_w, MAX_PATH, L"%s\\%s", exe_dir_w, ffd.cFileName);
        pdf_fullpath_w[MAX_PATH - 1] = 0;

        wprintf(L"Converting: %ls\n", pdf_fullpath_w);
        if (convert_one_pdf(ctx, pdf_fullpath_w, exe_dir_w))
            any = 1;

    } while (FindNextFileW(h, &ffd));

    FindClose(h);

    if (!any)
        printf("Done, but no conversions succeeded.\n");
    else
        printf("Done.\n");

    fz_drop_context(ctx);
    free(exe_dir_w);
    return 0;
}
