#include "hace/corba_exception.h"

static hace_corba_status_t hace_corba_from_cdr_status(hace_cdr_status_t status)
{
    switch (status) {
        case HACE_CDR_OK:
            return HACE_CORBA_OK;
        case HACE_CDR_ERR_BUFFER_OVERFLOW:
            return HACE_CORBA_ERR_BUFFER_OVERFLOW;
        case HACE_CDR_ERR_BUFFER_UNDERFLOW:
            return HACE_CORBA_ERR_BUFFER_UNDERFLOW;
        case HACE_CDR_ERR_INVALID_ARGUMENT:
            return HACE_CORBA_ERR_INVALID_ARGUMENT;
        default:
            return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
}

hace_corba_status_t hace_corba_encode_system_exception(
    hace_cdr_buffer_t *buf, const hace_corba_system_exception_t *exc)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || (exc == NULL)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_encode_u32(buf, (uint32_t)exc->exception);
    if (status != HACE_CDR_OK) {
        return hace_corba_from_cdr_status(status);
    }
    status = hace_cdr_encode_u32(buf, exc->minor_code);
    if (status != HACE_CDR_OK) {
        return hace_corba_from_cdr_status(status);
    }
    status = hace_cdr_encode_u32(buf, (uint32_t)exc->completion_status);
    return hace_corba_from_cdr_status(status);
}

hace_corba_status_t hace_corba_decode_system_exception(
    hace_cdr_buffer_t *buf, hace_corba_system_exception_t *out_exc)
{
    uint32_t exception_raw;
    uint32_t minor_code;
    uint32_t completion_raw;
    hace_cdr_status_t status;

    if ((buf == NULL) || (out_exc == NULL)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_decode_u32(buf, &exception_raw);
    if (status != HACE_CDR_OK) {
        return hace_corba_from_cdr_status(status);
    }
    if (exception_raw > (uint32_t)HACE_CORBA_EXC_COMM_FAILURE) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_decode_u32(buf, &minor_code);
    if (status != HACE_CDR_OK) {
        return hace_corba_from_cdr_status(status);
    }

    status = hace_cdr_decode_u32(buf, &completion_raw);
    if (status != HACE_CDR_OK) {
        return hace_corba_from_cdr_status(status);
    }
    if (completion_raw > (uint32_t)HACE_CORBA_COMPLETED_MAYBE) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    out_exc->exception = (hace_corba_exception_t)exception_raw;
    out_exc->minor_code = minor_code;
    out_exc->completion_status = (hace_corba_completion_status_t)completion_raw;
    return HACE_CORBA_OK;
}

hace_corba_system_exception_t hace_corba_exception_from_uiop_status(
    hace_uiop_status_t status)
{
    hace_corba_system_exception_t exc;

    exc.minor_code = 0U;

    switch (status) {
        case HACE_UIOP_OK:
            exc.exception = HACE_CORBA_EXC_NONE;
            exc.completion_status = HACE_CORBA_COMPLETED_YES;
            break;
        case HACE_UIOP_ERR_TIMEOUT:
            exc.exception = HACE_CORBA_EXC_TIMEOUT;
            exc.completion_status = HACE_CORBA_COMPLETED_MAYBE;
            break;
        case HACE_UIOP_ERR_HAL:
            exc.exception = HACE_CORBA_EXC_COMM_FAILURE;
            exc.completion_status = HACE_CORBA_COMPLETED_MAYBE;
            break;
        case HACE_UIOP_ERR_TOO_LARGE:
        case HACE_UIOP_ERR_INVALID_ARGUMENT:
            exc.exception = HACE_CORBA_EXC_BAD_PARAM;
            exc.completion_status = HACE_CORBA_COMPLETED_NO;
            break;
        default:
            exc.exception = HACE_CORBA_EXC_UNKNOWN;
            exc.completion_status = HACE_CORBA_COMPLETED_MAYBE;
            break;
    }

    return exc;
}
