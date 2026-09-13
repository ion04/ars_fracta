#pragma once

#include <cmath>
#include <ostream>

namespace ars::core::math {

/**
 * @brief Работа с комплексными числами (для итеративных фракталов).
 *
 * Заголовочный класс, не требует отдельного .cpp.
 */
class Complex {
public:
    double re{0.0};
    double im{0.0};

    constexpr Complex() = default;
    constexpr Complex(double re_, double im_) : re(re_), im(im_) {}

    constexpr Complex operator+(const Complex& o) const { return {re + o.re, im + o.im}; }
    constexpr Complex operator-(const Complex& o) const { return {re - o.re, im - o.im}; }
    constexpr Complex operator*(const Complex& o) const {
        return {re * o.re - im * o.im, re * o.im + im * o.re};
    }
    constexpr Complex operator*(double s) const { return {re * s, im * s}; }
    constexpr Complex operator/(double s) const { return {re / s, im / s}; }

    constexpr Complex& operator+=(const Complex& o) { return *this = *this + o; }
    constexpr Complex& operator-=(const Complex& o) { return *this = *this - o; }

    constexpr double magnitudeSquared() const { return re * re + im * im; }
    double magnitude() const { return std::sqrt(magnitudeSquared()); }
    double argument() const { return std::atan2(im, re); }

    constexpr Complex conjugate() const { return {re, -im}; }
    constexpr Complex squared() const { return *this * *this; }

    /// Возведение в степень n (для фракталов Жюлиа/Мандельброта).
    Complex pow(double n) const {
        const double r = magnitude();
        if (r < 1e-15) return {};
        const double theta = argument() * n;
        const double rn = std::pow(r, n);
        return {rn * std::cos(theta), rn * std::sin(theta)};
    }

    static Complex fromPolar(double r, double theta) {
        return {r * std::cos(theta), r * std::sin(theta)};
    }

    friend constexpr bool operator==(const Complex& a, const Complex& b) {
        return a.re == b.re && a.im == b.im;
    }

    friend std::ostream& operator<<(std::ostream& os, const Complex& z) {
        os << '(' << z.re << ", " << z.im << ')';
        return os;
    }
};

} // namespace ars::core::math