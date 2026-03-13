/**
 * @file    Compiler.h
 * @brief   AUTOSAR compiler abstraction macros
 * @details Provides compiler-independent qualifier macros.
 *
 * @copyright Taktflow Systems 2026
 * @project   Taktflow Embedded — Zonal Vehicle Platform
 * @standard  AUTOSAR R22-11, ISO 26262 Part 6
 */
#ifndef COMPILER_H
#define COMPILER_H

/* Memory class qualifiers -- simplified for GCC/ARM */
#define AUTOMATIC
#define STATIC      static
#define INLINE      static inline

/* NULL statement */
#define NULL_FUNC   ((void (*)(void))0)

/* Function pointer type */
#define P2FUNC(rettype, ptrclass, fctname)  rettype (*fctname)
#define CONSTP2FUNC(rettype, ptrclass, fctname) rettype (* const fctname)

/* Pointer types */
#define P2VAR(ptrtype, memclass, ptrclass)      ptrtype *
#define P2CONST(ptrtype, memclass, ptrclass)    const ptrtype *
#define CONSTP2VAR(ptrtype, memclass, ptrclass) ptrtype * const
#define CONSTP2CONST(ptrtype, memclass, ptrclass) const ptrtype * const

/* Function declaration */
#define FUNC(rettype, memclass) rettype

#endif /* COMPILER_H */
