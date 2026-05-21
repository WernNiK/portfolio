// Global motion system: reveal on view + hover interactions
(function () {
    const revealSelectors = [
        '.hero-greeting',
        '.hero-title',
        '.hero-subtitle',
        '.hero-cta',
        '.about-tabs',
        '.about-title',
        '.about-text',
        '.about-image',
        '.experience-item',
        '.portfolio-title',
        '.project-card',
        '.services-tabs',
        '.service-item',
        '.skill-item',
        '.contact-title',
        '.contact-subtitle',
        '.contact-email',
        '.social-icon'
    ];

    const revealNodes = document.querySelectorAll(revealSelectors.join(', '));

    revealNodes.forEach((node, index) => {
        node.classList.add('scroll-reveal');
        node.style.setProperty('--reveal-delay', `${Math.min(index * 35, 420)}ms`);
    });

    const observer = new IntersectionObserver(
        (entries) => {
            entries.forEach((entry) => {
                if (entry.isIntersecting) {
                    entry.target.classList.add('is-visible');
                    observer.unobserve(entry.target);
                }
            });
        },
        {
            threshold: 0.12,
            rootMargin: '0px 0px -8% 0px'
        }
    );

    revealNodes.forEach((node) => observer.observe(node));

    const hoverFloatSelectors = [
        '.hero-cta',
        '.about-tab',
        '.contact-email',
        '.social-icon',
        '.project-modal-cta',
        '.tag'
    ];

    document.querySelectorAll(hoverFloatSelectors.join(', ')).forEach((node) => {
        node.classList.add('hover-float');
    });

    const tiltSelectors = [
        '.project-card',
        '.service-item',
        '.skill-item',
        '.experience-item',
        '.about-image'
    ];

    document.querySelectorAll(tiltSelectors.join(', ')).forEach((node) => {
        node.classList.add('hover-tilt');

        node.addEventListener('mousemove', (event) => {
            const rect = node.getBoundingClientRect();
            const px = (event.clientX - rect.left) / rect.width;
            const py = (event.clientY - rect.top) / rect.height;
            const tiltY = (px - 0.5) * 5;
            const tiltX = (0.5 - py) * 5;

            node.style.setProperty('--tilt-x', `${tiltX.toFixed(2)}deg`);
            node.style.setProperty('--tilt-y', `${tiltY.toFixed(2)}deg`);
        });

        node.addEventListener('mouseleave', () => {
            node.style.setProperty('--tilt-x', '0deg');
            node.style.setProperty('--tilt-y', '0deg');
        });
    });
})();
