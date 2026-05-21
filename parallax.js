// Parallax Effect Module
(function() {
    window.addEventListener('scroll', () => {
        const scrolled = window.pageYOffset;
        const heroBackground = document.querySelector('.hero-background');
        
        if (heroBackground) {
            heroBackground.style.transform = `translateY(calc(-50% + ${scrolled * 0.5}px))`;
        }
    });
})();
